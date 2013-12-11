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
#include <objects/variation/VariationException.hpp>

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
#include <misc/hgvs/hgvs_parser2.hpp>
#include <misc/hgvs/variation_util2.hpp>


BEGIN_NCBI_SCOPE

namespace variation {

#define HGVS_THROW(err_code, message) NCBI_THROW(CHgvsParser::CHgvsParserException, err_code, message)

#define HGVS_ASSERT_RULE(i, rule_id) \
    if((i->value.id()) != (SGrammar::rule_id))             \
    {HGVS_THROW(eGrammatic, "Unexpected rule " + CHgvsParser::SGrammar::s_GetRuleName(i->value.id()) ); }


CSafeStatic<CHgvsParser::SGrammar> CHgvsParser::s_grammar;

const char* CHgvsParser::SGrammar::s_rule_names[CHgvsParser::SGrammar::eNodeIds_SIZE] = 
{
    "NONE",
    "root",
    "list_delimiter",
    "list1a",
    "list2a",
    "list3a",
    "list1b",
    "list2b",
    "list3b",
    "expr1",
    "expr2",
    "expr3",
    "translocation",
    "header",
    "seq_id",
    "mut_list",
    "mut_ref",
    "mol",
    "int_fuzz",
    "abs_pos",
    "general_pos",
    "fuzzy_pos",
    "pos_spec",
    "location",
    "nuc_range",
    "prot_range",
    "mut_inst",
    "raw_seq",
    "raw_seq_or_len",
    "aminoacid1",
    "aminoacid2",
    "aminoacid3",
    "nuc_subst",
    "deletion",
    "insertion",
    "delins",
    "duplication",
    "nuc_inv",
    "ssr",
    "conversion",
    "seq_loc",
    "seq_ref",
    "prot_pos",
    "prot_fs",
    "prot_missense",
    "prot_ext",
    "no_change"
};





CVariantPlacement& SetFirstPlacement(CVariation& v)
{
    if(v.SetPlacements().size() == 0) {
        CRef<CVariantPlacement> p(new CVariantPlacement);
        v.SetPlacements().push_back(p);
    }
    return *v.SetPlacements().front();
}

void SetComputational(CVariation& variation)
{
    CVariationMethod& m = variation.SetMethod();
    m.SetMethod();

    if(find(m.GetMethod().begin(),
            m.GetMethod().end(),
            CVariationMethod::eMethod_E_computational) == m.GetMethod().end())
    {
        m.SetMethod().push_back(CVariationMethod::eMethod_E_computational);
    }
}


bool SeqsMatch(const string& query, const char* text)
{
    static const char* iupac_bases = ".TGKCYSBAWRDMHVN"; //position of the iupac literal = 4-bit mask for A|C|G|T
    for(size_t i = 0; i < query.size(); i++) {
        size_t a = CTempString(iupac_bases).find(query[i]);
        size_t b = CTempString(iupac_bases).find(text[i]);
        if(!(a & b)) {
            return false;
        }
    }
    return true;
}


CRef<CSeq_loc> FindSSRLoc(const CSeq_loc& loc, const string& seq, CScope& scope)
{
    //Extend the loc 10kb up and down; Find all occurences of seq in the resulting
    //interval, create locs for individual repeat units; then merge them, and keep the interval that
    //overlaps the original.

    const TSeqPos ext_interval = 10000;

    CRef<CSeq_loc> loc1 = sequence::Seq_loc_Merge(loc, CSeq_loc::fMerge_SingleRange, NULL);
    CBioseq_Handle bsh = scope.GetBioseqHandle(sequence::GetId(loc, NULL));
    TSeqPos seq_len = bsh.GetInst_Length();
    loc1->SetInt().SetFrom() -= min(ext_interval, loc1->GetInt().GetFrom());
    loc1->SetInt().SetTo() += min(ext_interval, seq_len - 1 - loc1->GetInt().GetTo());

    CSeqVector v(*loc1, scope, CBioseq_Handle::eCoding_Iupac);
    string str1;
    v.GetSeqData(v.begin(), v.end(), str1);

    CRef<CSeq_loc> container(new CSeq_loc(CSeq_loc::e_Mix));

    for(size_t i = 0; i < str1.size() - seq.size(); i++) {
        if(SeqsMatch(seq, &str1[i])) {
            CRef<CSeq_loc> repeat_unit_loc(new CSeq_loc);
            repeat_unit_loc->Assign(*loc1);

            if(sequence::GetStrand(loc, NULL) == eNa_strand_minus) {
                repeat_unit_loc->SetInt().SetTo() -= i;
                repeat_unit_loc->SetInt().SetFrom(repeat_unit_loc->GetInt().GetTo() - (seq.size() - 1));
            } else {
                repeat_unit_loc->SetInt().SetFrom() += i;
                repeat_unit_loc->SetInt().SetTo(repeat_unit_loc->GetInt().GetFrom() + (seq.size() - 1));
            }
            container->SetMix().Set().push_back(repeat_unit_loc);
        }
    }

    CRef<CSeq_loc> merged_repeats = sequence::Seq_loc_Merge(*container, CSeq_loc::fSortAndMerge_All, NULL);
    merged_repeats->ChangeToMix();
    CRef<CSeq_loc> result(new CSeq_loc(CSeq_loc::e_Null));
    result->Assign(loc);

    for(CSeq_loc_CI ci(*merged_repeats); ci; ++ci) {
        const CSeq_loc& loc2 = ci.GetEmbeddingSeq_loc();
        if(sequence::Compare(loc, loc2, NULL) != sequence::eNoOverlap) {
            result->Add(loc2);
        }
    }

    return sequence::Seq_loc_Merge(*result, CSeq_loc::fSortAndMerge_All, NULL);
}




void CHgvsParser::s_SetStartOffset(CVariantPlacement& p, const CHgvsParser::SFuzzyInt& fint)
{
    p.ResetStart_offset();
    p.ResetStart_offset_fuzz();
    if(fint.value || fint.fuzz) {
        p.SetStart_offset(fint.value);
    }
    if(fint.fuzz) {
        p.SetStart_offset_fuzz().Assign(*fint.fuzz);
    }
}

void CHgvsParser::s_SetStopOffset(CVariantPlacement& p, const CHgvsParser::SFuzzyInt& fint)
{
    p.ResetStop_offset();
    p.ResetStop_offset_fuzz();
    if(fint.value || fint.fuzz) {
        p.SetStop_offset(fint.value);
    }
    if(fint.fuzz) {
        p.SetStop_offset_fuzz().Assign(*fint.fuzz);
    }
}



//if a variation has an asserted sequence, stored in placement.seq, repackage it as a set having
//the original variation and a synthetic one representing the asserted sequence. The placement.seq
//is cleared, as it is a placeholder for the actual reference sequence.
void RepackageAssertedSequence(CVariation& vr)
{
    if(vr.IsSetPlacements() && SetFirstPlacement(vr).IsSetSeq()) {
        CRef<CVariation> container(new CVariation);
        container->SetPlacements() = vr.SetPlacements();

        CRef<CVariation> orig(new CVariation);
        orig->Assign(vr);
        orig->ResetPlacements(); //location will be set on the package, as it is the same for both members

        container->SetData().SetSet().SetType(CVariation::TData::TSet::eData_set_type_package);
        container->SetData().SetSet().SetVariations().push_back(orig);

        CRef<CVariation> asserted_vr(new CVariation);
        asserted_vr->SetData().SetInstance().SetObservation(CVariation_inst::eObservation_asserted);
        asserted_vr->SetData().SetInstance().SetType(CVariation_inst::eType_identity);

        CRef<CDelta_item> delta(new CDelta_item);
        delta->SetSeq().SetLiteral().Assign(SetFirstPlacement(vr).GetSeq());
        asserted_vr->SetData().SetInstance().SetDelta().push_back(delta);

        SetFirstPlacement(*container).ResetSeq();
        container->SetData().SetSet().SetVariations().push_back(asserted_vr);

        vr.Assign(*container);

    } else if(vr.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, vr.SetData().SetSet().SetVariations()) {
            RepackageAssertedSequence(**it);
        }
    }
}


//HGVS distinguished between c. n. and r. molecules, while in 
//VariantPlacement we have moltype cdna (which maps onto c.) and rna (which maps onto n.)
//
//If we have an HGVS expression like NM_123456.7:r. the VariantPlacement will have moltype rna
//that would map-back onto NM_123456.7:n., which is what we don't want, as "n.' reserved
//for non-coding RNAs, so we'll convert rna moltype to cdna based on accession here.
//Note that this is a post-processing step, as in the midst of parsing we want to treat
//NM_123456.7:r. as non-coding rna, since these have absolute coordinates rather than cds-relative.
void AdjustMoltype(CVariation& vr, CScope& scope)
{
    CVariationUtil util(scope);

    for(CTypeIterator<CVariantPlacement> it(Begin(vr)); it; ++it) {
        CVariantPlacement& p = *it;

        if(p.IsSetMol() 
           && p.GetMol() == CVariantPlacement::eMol_rna
           && p.GetLoc().GetId())
        {
            p.SetMol(util.GetMolType(*p.GetLoc().GetId()));
        }
    }
}


CHgvsParser::CContext::CContext(const CContext& other)
  : m_hgvs(other.m_hgvs)
{
    this->m_bsh = other.m_bsh;
    this->m_cds = other.m_cds;
    this->m_scope = other.m_scope;
    this->m_seq_id_resolvers = other.m_seq_id_resolvers;
    this->m_placement.Reset();
    if(other.m_placement) {
        /*
         * Note: need to make a copy of the placement, such that if preceding subvariation
         * is location-specific and the other one isn't, first's placement is not
         * given to the other one, e.g. "NM_004004.2:c.[35_36dup;40del]+[=]" - if we
         * made a shallow copy, then the location context for "[=]" would be erroneously
         * inherited from the last sibling: "NM_004004.2:c.40=" instead of whole "NM_004004.2:c.="
         */
         this->m_placement.Reset(new CVariantPlacement);
         this->m_placement->Assign(*other.m_placement);
    }
}

const CSeq_feat& CHgvsParser::CContext::GetCDS() const
{
    if(m_cds.IsNull()) {
        HGVS_THROW(eContext, "No CDS feature in context");
    }
    return *m_cds;
}

const CSeq_id& CHgvsParser::CContext::GetId() const
{
    return sequence::GetId(GetPlacement().GetLoc(), NULL);
}


void CHgvsParser::CContext::SetId(const CSeq_id& id, CVariantPlacement::TMol mol)
{
    Clear();

    SetPlacement().SetMol(mol);
    SetPlacement().SetLoc().SetWhole().Assign(id);

    m_bsh = m_scope->GetBioseqHandle(id);

    if(!m_bsh) {
        HGVS_THROW(eContext, "Cannnot get bioseq for seq-id " + id.AsFastaString());
    }

    if(mol == CVariantPlacement::eMol_cdna) {
        SAnnotSelector sel;
        sel.SetResolveTSE();
        for(CFeat_CI ci(m_bsh, sel); ci; ++ci) {
            const CMappedFeat& mf = *ci;
            if(mf.GetData().IsCdregion()) {
                if(m_cds.IsNull()) {
                    m_cds.Reset(new CSeq_feat());
                    m_cds->Assign(mf.GetMappedFeature());
                } else {
                    HGVS_THROW(eContext, "Multiple CDS features on the sequence");
                }
            }
        }
        if(m_cds.IsNull()) {
            HGVS_THROW(eContext, "Could not find CDS feat");
        }
    }
}


const string CHgvsParser::SGrammar::s_GetRuleName(parser_id id)
{
    if(id.to_long() >= CHgvsParser::SGrammar::eNodeIds_SIZE) {
        HGVS_THROW(eLogic, "Rule name not hardcoded");
    } else {
        return s_rule_names[id.to_long()];
    }
}


CHgvsParser::SFuzzyInt CHgvsParser::x_int_fuzz(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_int_fuzz);
    TIterator it = i->children.begin();

    CRef<CInt_fuzz> fuzz(new CInt_fuzz);
    int value = 0;

    if(i->children.size() == 1) { //e.g. '5' or '?'
        string s(it->value.begin(), it->value.end());
        if(s == "?") {
            value = 0;
            fuzz->SetLim(CInt_fuzz::eLim_unk);
        } else {
            value = NStr::StringToInt(s);
            fuzz.Reset();
        }
    } else if(i->children.size() == 3) { //e.g. '(5)' or '(?)'
        ++it;
        string s(it->value.begin(), it->value.end());
        if(s == "?") {
            value = 0;
            fuzz->SetLim(CInt_fuzz::eLim_unk);
        } else {
            value = NStr::StringToInt(s);
            fuzz->SetLim(CInt_fuzz::eLim_unk);
        }
    } else if(i->children.size() == 5) { //e.g. '(5_7)' or '(?_10)'
        ++it;
        string s1(it->value.begin(), it->value.end());
        ++it;
        ++it;
        string s2(it->value.begin(), it->value.end());

        if(s1 == "?" && s2 == "?") {
            value = 0;
            fuzz->SetLim(CInt_fuzz::eLim_unk);
        } else if(s1 != "?" && s2 != "?") {
            value = NStr::StringToInt(s1);
            fuzz->SetRange().SetMin(NStr::StringToInt(s1));
            fuzz->SetRange().SetMax(NStr::StringToInt(s2));
        } else if(s2 == "?") {
            value = NStr::StringToInt(s1);
            fuzz->SetLim(CInt_fuzz::eLim_gt);
        } else if(s1 == "?") {
            value = NStr::StringToInt(s2);
            fuzz->SetLim(CInt_fuzz::eLim_lt);
        } else {
            HGVS_THROW(eLogic, "Unreachable code");
        }
    }

    CHgvsParser::SFuzzyInt fuzzy_int;
    fuzzy_int.value = value;
    fuzzy_int.fuzz = fuzz;

#if 0
    NcbiCerr << "Fuzzy int: " << value << " ";
    if(fuzz) {
        NcbiCerr << MSerial_AsnText << *fuzz;
    } else {
        NcbiCerr << "\n";
    }
#endif

    return fuzzy_int;
}



/* In HGVS:
 * the nucleotide 3' of the translation stop codon is *1, the next *2, etc.
 * # there is no nucleotide 0
 * # nucleotide 1 is the A of the ATG-translation initiation codon
 * # the nucleotide 5' of the ATG-translation initiation codon is -1, the previous -2, etc.
 *
 * I.e. need to adjust if dealing with positive coordinates, except for *-relative ones.
 */
template<typename T>
T AdjustHgvsCoord(T val, size_t offset, bool adjust)
{
    val += offset;
    if(adjust && val > offset) { 
        // note: val may be unsigned, so check for (val+offset > offset)
        // instead of (val > 0)
        val -= 1;
    }
    return val;
}

CRef<CSeq_point> CHgvsParser::x_abs_pos(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_abs_pos);
    TIterator it = i->children.begin();

    TSignedSeqPos offset(0);
    bool adjust = true; //see AdjustHgvsCoord comments above
    if(i->children.size() == 2) {
        adjust = false;
        string s(it->value.begin(), it->value.end());
        if(s != "*") {
            HGVS_THROW(eGrammatic, "Expected literal '*'");
        }
        if(context.GetPlacement().GetMol() != CVariantPlacement::eMol_cdna) {
            HGVS_THROW(eContext, "Expected 'c.' context for stop-codon-relative coordinate");
        }

        offset = context.GetCDS().GetLocation().GetStop(eExtreme_Biological);
        ++it;
    } else {
        if (context.GetPlacement().GetMol() == CVariantPlacement::eMol_cdna) {
            //Note: in RNA coordinates (r.) the coordinates are absolute, like in genomic sequences,
            //  "The RNA sequence type uses only GenBank mRNA records. The value 1 is assigned to the first
            //  base in the record and from there all bases are counted normally."
            //so the cds-start offset applies only to "c." coordinates
            offset = context.GetCDS().GetLocation().GetStart(eExtreme_Biological);
        }
    }

    CRef<CSeq_point> pnt(new CSeq_point);
    {{
        SFuzzyInt int_fuzz = x_int_fuzz(it, context);

        pnt->SetId().Assign(context.GetId());
        pnt->SetStrand(eNa_strand_plus);
        pnt->SetPoint(AdjustHgvsCoord(int_fuzz.value, offset, adjust));

        if(!int_fuzz.fuzz.IsNull()) {
            pnt->SetFuzz(*int_fuzz.fuzz);
            if(pnt->GetFuzz().IsRange()) {
                CInt_fuzz_Base::TRange& r = pnt->SetFuzz().SetRange();
                r.SetMin(AdjustHgvsCoord(r.GetMin(), offset, adjust));
                r.SetMax(AdjustHgvsCoord(r.GetMax(), offset, adjust));
            }
        }
    }}

    return pnt;
}


/*
 * general_pos is either simple abs-pos that is passed down to x_abs_pos,
 * or an intronic location that is specified by a mapping point in the
 * local coordinates and the -upstream / +downstream offset after remapping.
 *
 * The mapping point can either be an abs-pos in local coordinates, or
 * specified as offset in intron-specific coordinate system where IVS# specifies
 * the intron number
 */
CHgvsParser::SOffsetPoint CHgvsParser::x_general_pos(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_general_pos);

    SOffsetPoint ofpnt;

    if(i->children.size() == 1) {
        //local coordinates
        ofpnt.pnt = x_abs_pos(i->children.begin(), context);
    } else {
        //(str_p("IVS") >> int_p | abs_pos) >> sign_p >> int_fuzz

        TIterator it = i->children.end() - 1;
        ofpnt.offset = x_int_fuzz(it, context);
        --it;

        //adjust for sign; convert +? or -? offsets to 
        //0> or 0<
        string s_sign(it->value.begin(), it->value.end());
        int sign1 = s_sign == "-" ? -1 : 1;
        ofpnt.offset.value *= sign1;
        if(ofpnt.offset.fuzz && ofpnt.offset.fuzz->IsRange()) {
            ofpnt.offset.fuzz->SetRange().SetMin() *= sign1;
            ofpnt.offset.fuzz->SetRange().SetMax() *= sign1;
        } else if(ofpnt.offset.fuzz &&
           ofpnt.offset.value == 0 &&     
           ofpnt.offset.fuzz->IsLim() &&
           ofpnt.offset.fuzz->GetLim() == CInt_fuzz::eLim_unk)
        {
            ofpnt.offset.fuzz->SetLim(sign1 < 0 ? CInt_fuzz::eLim_lt : CInt_fuzz::eLim_gt);
        }

        --it;
        if(it->value.id() == SGrammar::eID_abs_pos) {
            //base-loc is an abs-pos
            ofpnt.pnt = x_abs_pos(i->children.begin(), context);
        } else {
            //base-loc is IVS-relative.
            ofpnt.pnt.Reset(new CSeq_point);
            ofpnt.pnt->SetId().Assign(context.GetId());
            ofpnt.pnt->SetStrand(eNa_strand_plus);

            TIterator it = i->children.begin();
            string s_ivs(it->value.begin(), it->value.end());
            ++it;
            string s_ivs_num(it->value.begin(), it->value.end());
            int ivs_num = NStr::StringToInt(s_ivs_num);

            //If IVS3+50, the mapping point is the last base of third exon
            //if IVS3-50, the mapping point is the first base of the fourth exon
            size_t target_exon_num = sign1 < 0 ? ivs_num + 1 : ivs_num;

            SAnnotSelector sel;
            sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_exon);
            CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(context.GetId());
            size_t exon_num = 1;
            //Note: IVS is cDNA-centric, so we'll have to use ordinals of the exons instead of /number qual
            for(CFeat_CI ci(bsh, sel); ci; ++ci) {
                const CMappedFeat& mf = *ci;
                if(exon_num == target_exon_num) {
                    ofpnt.pnt->SetPoint(sign1 > 0 ? mf.GetLocation().GetStop(eExtreme_Biological)
                                                  : mf.GetLocation().GetStart(eExtreme_Biological));
                    break;
                }
                exon_num++;
            }
        }
    }

    //We could be dealing with improper HGVS expression (that we need to support anyway)
    //where the coordinate extends beyond the range of sequence
    //e.g. NM_000518:c.-78A>G, where codon-start is at 51. In this case the resulting
    //coordinate will be negative; we'll convert it to offset-format (27 bases upstream of pos 0)

    if(context.GetPlacement().GetMol()== CVariantPlacement::eMol_cdna || context.GetPlacement().GetMol() == CVariantPlacement::eMol_rna) {
        if(static_cast<TSignedSeqPos>(ofpnt.pnt->GetPoint()) < 0) {
            ofpnt.offset.value += static_cast<TSignedSeqPos>(ofpnt.pnt->GetPoint());
            ofpnt.pnt->SetPoint(0);
        } else if(ofpnt.pnt->GetPoint() >= context.GetBioseqHandle().GetInst_Length()) {
            //in case there's overrun past the end of the sequence, set the anchor as
            //last position of the last point of the last exon (NOT last point of the sequence,
            //as we could end up in polyA).

            TSeqPos anchor_pos = 0;
            SAnnotSelector sel;
            sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_exon);
            for(CFeat_CI ci(context.GetBioseqHandle(), sel); ci; ++ci) {
                const CMappedFeat& mf = *ci;
                anchor_pos = max(anchor_pos, mf.GetLocation().GetStop(eExtreme_Positional));
            }

            //didn't find any exons - so set the anchor point as last point in the sequence
            if(anchor_pos == 0) {
                anchor_pos = context.GetBioseqHandle().GetInst_Length() - 1;
            }

            TSeqPos overrun = ofpnt.pnt->GetPoint() - anchor_pos;
            ofpnt.offset.value += overrun;
            ofpnt.pnt->SetPoint(anchor_pos);
        }
    }

    return ofpnt;
}


CHgvsParser::SOffsetPoint CHgvsParser::x_fuzzy_pos(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_fuzzy_pos);

    SOffsetPoint pnt;
    SOffsetPoint pnt1 = x_general_pos(i->children.begin(), context);
    SOffsetPoint pnt2 = x_general_pos(i->children.begin() + 1, context);

    //Verify that on the same seq-id.
    if(!pnt1.pnt->GetId().Equals(pnt2.pnt->GetId())) {
        HGVS_THROW(eSemantic, "Points in a fuzzy pos are on different sequences");
    }
    if(pnt1.pnt->GetStrand() != pnt2.pnt->GetStrand()) {
        HGVS_THROW(eSemantic, "Range-loc start/stop are on different strands.");
    }

    //If One is empty, copy from the other and set TL for loc1 and TR for loc2
    if(pnt1.pnt->GetPoint() == kInvalidSeqPos && pnt2.pnt->GetPoint() != kInvalidSeqPos) {
        pnt1.pnt->Assign(*pnt2.pnt);
        pnt1.pnt->SetFuzz().SetLim(CInt_fuzz::eLim_tl);
    } else if(pnt1.pnt->GetPoint() != kInvalidSeqPos && pnt2.pnt->GetPoint() == kInvalidSeqPos) {
        pnt2.pnt->Assign(*pnt1.pnt);
        pnt2.pnt->SetFuzz().SetLim(CInt_fuzz::eLim_tr);
    }

    if((pnt1.offset.value != 0 || pnt2.offset.value != 0) && !pnt1.pnt->Equals(*pnt2.pnt)) {
        HGVS_THROW(eSemantic, "Base-points in an intronic fuzzy position must be equal");
    }

    pnt.pnt = pnt1.pnt;
    pnt.offset = pnt1.offset;

    if(pnt1.offset.value != pnt2.offset.value) {
        pnt.offset.fuzz.Reset(new CInt_fuzz);
        pnt.offset.fuzz->SetRange().SetMin(pnt1.offset.value);
        pnt.offset.fuzz->SetRange().SetMax(pnt2.offset.value);
    }

    return pnt;

#if 0
    todo: reconcile
    //If Both are Empty - the result is empty, otherwise reconciliate
    if(pnt1.pnt->GetPoint() == kInvalidSeqPos && pnt2.pnt->GetPoint() == kInvalidSeqPos) {
        pnt.pnt = pnt1.pnt;
        pnt.offset = pnt1.offset;
    } else {
        pnt.pnt.Reset(new CSeq_point);
        pnt.pnt.Assign(*pnt1.pnt);

        TSeqPos min_pos = min(pnt1.pnt->GetPoint(), pnt2.pnt->GetPoint());
        TSeqPos max_pos = max(pnt1.pnt->GetPoint(), pnt2.pnt->GetPoint());

        if(!pnt1->IsSetFuzz() && !pnt2->IsSetFuzz()) {
            //Both are non-fuzzy - create the min-max fuzz.
            //(10+50_10+60)
            pnt->SetFuzz().SetRange().SetMin(min_pos);
            pnt->SetFuzz().SetRange().SetMax(max_pos);

        } else if(pnt1->IsSetFuzz() && pnt2->IsSetFuzz()) {
            //Both are fuzzy - reconcile the fuzz.

            if(pnt1->GetFuzz().GetLim() == CInt_fuzz::eLim_tr
            && pnt2->GetFuzz().GetLim() == CInt_fuzz::eLim_tl)
            {
                //fuzz points inwards - create min-max fuzz
                //(10+?_11-?)
                pnt->SetFuzz().SetRange().SetMin(min_pos);
                pnt->SetFuzz().SetRange().SetMax(max_pos);

            } else if (pnt1->GetFuzz().GetLim() == CInt_fuzz::eLim_tl
                    && pnt2->GetFuzz().GetLim() == CInt_fuzz::eLim_tr)
            {
                //fuzz points outwards - set fuzz to unk
                //(10-?_10+?)
                //(?_10+?)
                //(10-?_?)
                pnt->SetFuzz().SetLim(CInt_fuzz::eLim_unk);

            }  else if (pnt1->GetFuzz().GetLim() == CInt_fuzz::eLim_tl
                     && pnt2->GetFuzz().GetLim() == CInt_fuzz::eLim_tl)
            {
                //fuzz is to the left - use 5'-most
                //(?_10-?)
                //(10-?_11-?)
                pnt->SetPoint(pnt->GetStrand() == eNa_strand_minus ? max_pos : min_pos);

            }  else if (pnt1->GetFuzz().GetLim() == CInt_fuzz::eLim_tr
                     && pnt2->GetFuzz().GetLim() == CInt_fuzz::eLim_tr)
            {
                //fuzz is to the right - use 3'-most
                //(10+?_?)
                //(10+?_11+?)
                pnt->SetPoint(pnt->GetStrand() == eNa_strand_minus ? min_pos : max_pos);

            } else {
                pnt->SetFuzz().SetLim(CInt_fuzz::eLim_unk);
            }
        } else {
            // One of the two is non-fuzzy:
            // use it to specify position, and the fuzz of the other to specify the fuzz
            // e.g.  (10+5_10+?)  -> loc1=100005; loc2=100000tr  -> 100005tr

            pnt->Assign(pnt1->IsSetFuzz() ? *pnt2 : *pnt1);
            pnt->SetFuzz().Assign(pnt1->IsSetFuzz() ? pnt1->GetFuzz()
                                                    : pnt2->GetFuzz());

        }
    }
#endif



}


CHgvsParser::CContext CHgvsParser::x_header(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_header);

    CContext ctx(context);

    TIterator it = i->children.rbegin()->children.begin();
    string mol(it->value.begin(), it->value.end());
    CVariantPlacement::TMol mol_type =
                       mol == "c" ? CVariantPlacement::eMol_cdna
                     : mol == "g" ? CVariantPlacement::eMol_genomic
                     : mol == "r" ? CVariantPlacement::eMol_rna
                     : mol == "n" ? CVariantPlacement::eMol_rna
                     : mol == "p" ? CVariantPlacement::eMol_protein
                     : mol == "m" ? CVariantPlacement::eMol_mitochondrion
                     : mol == "mt" ? CVariantPlacement::eMol_mitochondrion
                     : CVariantPlacement::eMol_unknown;

    it  = (i->children.rbegin() + 1)->children.begin();
    string id_str(it->value.begin(), it->value.end());

    CSeq_id_Handle idh = context.ResolevSeqId(id_str);
    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(idh);
    if(!bsh) {
        HGVS_THROW(eSemantic, "Could not resolve seq-id " + id_str);
    }
        

    if(bsh.IsNucleotide() 
       && mol_type == CVariantPlacement::eMol_protein 
       && NStr::Find(id_str, "CCDS") == 0) 
    {
        //If we have something like CCDS2.1:p., the CCDS2.1 will resolve
        //to an NM, but we need to resolve it to corresponding NP.
        //
        //We could do this for all seq-ids, as long as there's unique CDS,
        //but as per SNP-4536 the seq-id and moltype correspondence must be enforced,
        //so we do it for CCDS only

        SAnnotSelector sel;
        sel.SetResolveTSE();
        sel.IncludeFeatType(CSeqFeatData::e_Cdregion);
        bool already_found = false;
        for(CFeat_CI ci(bsh, sel); ci; ++ci) {
            const CMappedFeat& mf = *ci;
            if(mf.IsSetProduct() && mf.GetProduct().GetId()) {
                if(already_found) {
                    HGVS_THROW(eSemantic, "Can't resolve to prot - multiple CDSes on " + idh.AsString());
                } else {
                    idh = sequence::GetId(*mf.GetProduct().GetId(), context.GetScope(), sequence::eGetId_ForceAcc);
                    already_found = true;
                }
            }
        }
        if(!already_found) {
            HGVS_THROW(eSemantic, "Can't resolve to prot - can't find CDS on " + idh.AsString());
        }
    }

    ctx.SetId(*idh.GetSeqId(), mol_type);

    if(i->children.size() == 3) {
        it  = (i->children.rbegin() + 2)->children.begin();
        string tag_str(it->value.begin(), it->value.end());
        //record tag in context, if it is necessary in the future
    }

    return ctx;
}


CHgvsParser::SOffsetPoint CHgvsParser::x_pos_spec(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_pos_spec);

    SOffsetPoint pnt;
    TIterator it = i->children.begin();
    if(it->value.id() == SGrammar::eID_general_pos) {
        pnt = x_general_pos(it, context);
    } else if(it->value.id() == SGrammar::eID_fuzzy_pos) {
        pnt = x_fuzzy_pos(it, context);
    } else {
        bool flip_strand = false;
        if(i->children.size() == 3) {
            //first child is 'o' - opposite
            flip_strand = true;
            ++it;
        }

        CContext local_ctx = x_header(it, context);
        ++it;
        pnt = x_pos_spec(it, local_ctx);

        if(flip_strand) {
            pnt.pnt->FlipStrand();
        }
    }

    return pnt;
}


CHgvsParser::SOffsetPoint CHgvsParser::x_prot_pos(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_prot_pos);
    TIterator it = i->children.begin();

    CRef<CSeq_literal> prot_literal = x_raw_seq(it, context);

    if(context.GetPlacement().GetMol() != CVariantPlacement::eMol_protein) {
        HGVS_THROW(eSemantic, "Expected protein context");
    }

    if(prot_literal->GetLength() != 1) {
        HGVS_THROW(eSemantic, "Expected single aa literal in prot-pos");
    }

    ++it;
    SOffsetPoint pnt = x_pos_spec(it, context);

    pnt.asserted_sequence = prot_literal->GetSeq_data().GetNcbieaa();

    return pnt;
}


CRef<CVariantPlacement> CHgvsParser::x_range(TIterator const& i, const CContext& context)
{
    SOffsetPoint pnt1, pnt2;

    CRef<CVariantPlacement> p(new CVariantPlacement);
    p->Assign(context.GetPlacement());

    if(i->value.id() == SGrammar::eID_prot_range) {
        pnt1 = x_prot_pos(i->children.begin(), context);
        pnt2 = x_prot_pos(i->children.begin() + 1, context);
    } else if(i->value.id() == SGrammar::eID_nuc_range) {
        pnt1 = x_pos_spec(i->children.begin(), context);
        pnt2 = x_pos_spec(i->children.begin() + 1, context);
    } else {
        HGVS_ASSERT_RULE(i, eID_NONE);
    }

    if(!pnt1.pnt->GetId().Equals(pnt2.pnt->GetId())) {
        HGVS_THROW(eSemantic, "Range-loc start/stop are on different seq-ids.");
    }
    if(pnt1.pnt->GetStrand() != pnt2.pnt->GetStrand()) {
        HGVS_THROW(eSemantic, "Range-loc start/stop are on different strands.");
    }

    p->SetLoc().SetInt().SetId(pnt1.pnt->SetId());
    p->SetLoc().SetInt().SetFrom(pnt1.pnt->GetPoint());
    p->SetLoc().SetInt().SetTo(pnt2.pnt->GetPoint());
    p->SetLoc().SetInt().SetStrand(pnt1.pnt->GetStrand());
    if(pnt1.pnt->IsSetFuzz()) {
        p->SetLoc().SetInt().SetFuzz_from(pnt1.pnt->SetFuzz());
    }

    if(pnt2.pnt->IsSetFuzz()) {
        p->SetLoc().SetInt().SetFuzz_to(pnt2.pnt->SetFuzz());
    }

    s_SetStartOffset(*p, pnt1.offset);
    s_SetStopOffset(*p, pnt2.offset);

    if(pnt1.asserted_sequence != "" || pnt2.asserted_sequence != "") {
        //for proteins, the asserted sequence is specified as part of location, rather than variation
        p->SetSeq().SetLength(CVariationUtil::s_GetLength(*p, NULL));
        string& seq_str = (context.GetPlacement().GetMol() == CVariantPlacement::eMol_protein)
                ? p->SetSeq().SetSeq_data().SetNcbieaa().Set()
                : p->SetSeq().SetSeq_data().SetIupacna().Set();
        seq_str = pnt1.asserted_sequence + ".." + pnt2.asserted_sequence;
    }

    return p;
}

CRef<CVariantPlacement> CHgvsParser::x_location(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_location);

    CRef<CVariantPlacement> placement(new CVariantPlacement);
    placement->Assign(context.GetPlacement());

    TIterator it = i->children.begin();
    CRef<CSeq_loc> loc(new CSeq_loc);
    if(it->value.id() == SGrammar::eID_prot_pos || it->value.id() == SGrammar::eID_pos_spec) {
        SOffsetPoint pnt = it->value.id() == SGrammar::eID_prot_pos
                ? x_prot_pos(it, context)
                : x_pos_spec(it, context);
        placement->SetLoc().SetPnt(*pnt.pnt);
        s_SetStartOffset(*placement, pnt.offset);
        if(pnt.asserted_sequence != "") {
            placement->SetSeq().SetLength(CVariationUtil::s_GetLength(*placement, NULL));
            string& seq_str = (context.GetPlacement().GetMol() == CVariantPlacement::eMol_protein)
                    ? placement->SetSeq().SetSeq_data().SetNcbieaa().Set()
                    : placement->SetSeq().SetSeq_data().SetIupacna().Set();
            seq_str = pnt.asserted_sequence;
        }
        
        //todo point with pos=0 and fuzz=unk -> unknown pos ->set loc to empty

    } else if(it->value.id() == SGrammar::eID_nuc_range || it->value.id() == SGrammar::eID_prot_range) {
        placement = x_range(it, context);
    } else {
        HGVS_ASSERT_RULE(it, eID_NONE);
    }

    if(placement->GetLoc().IsPnt() && placement->GetLoc().GetPnt().GetPoint() == kInvalidSeqPos) {
        placement->SetLoc().SetEmpty().Assign(context.GetId());
    }

    CVariationUtil util(context.GetScope());
    if(CVariationUtil::eFail == util.CheckExonBoundary(*placement)) {
        CRef<CVariationException> exception(new CVariationException);
        exception->SetCode(CVariationException::eCode_hgvs_exon_boundary);
        exception->SetMessage("HGVS exon-boundary position not represented in the transcript annotation");
        placement->SetExceptions().push_back(exception);    
    }
    util.CheckPlacement(*placement);

    return placement;
}


CRef<CSeq_loc> CHgvsParser::x_seq_loc(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_seq_loc);
    TIterator it = i->children.begin();

    bool flip_strand = false;
    if(i->children.size() == 3) {
        //first child is 'o' - opposite
        flip_strand = true;
        ++it;
    }

    CContext local_context = x_header(it, context);
    ++it;
    CRef<CVariantPlacement> p = x_location(it, local_context);

    if(flip_strand) {
        p->SetLoc().FlipStrand();
    }

    if(p->IsSetStop_offset() || p->IsSetStart_offset()) {
        HGVS_THROW(eSemantic, "Intronic seq-locs are not supported in this context");
    }

    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->Assign(p->GetLoc());
    return loc;
}

CRef<CSeq_literal> CHgvsParser::x_raw_seq_or_len(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_raw_seq_or_len);

    CRef<CSeq_literal> literal;
    TIterator it = i->children.begin();

    if(it == i->children.end()) {
        HGVS_THROW(eLogic, "Unexpected parse-tree state when parsing " + context.GetHgvs());
    }

    if(it->value.id() == SGrammar::eID_raw_seq) {
        literal = x_raw_seq(it, context);
    } else if(it->value.id() == SGrammar::eID_int_fuzz) {
        SFuzzyInt int_fuzz = x_int_fuzz(it, context);
        literal.Reset(new CSeq_literal);
        literal->SetLength(int_fuzz.value);
        if(int_fuzz.fuzz.IsNull()) {
            ;//no-fuzz;
        } else if(int_fuzz.fuzz->IsLim() && int_fuzz.fuzz->GetLim() == CInt_fuzz::eLim_unk) {
            //unknown length (no value) - will represent as length=0 with gt fuzz
            literal->SetFuzz().SetLim(CInt_fuzz::eLim_gt);
        } else {
            literal->SetFuzz(*int_fuzz.fuzz);
        }
    } else {
        HGVS_ASSERT_RULE(it, eID_NONE);
    }
    return literal;
}

CHgvsParser::TDelta CHgvsParser::x_seq_ref(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_seq_ref);
    CHgvsParser::TDelta delta(new TDelta::TObjectType);
    TIterator it = i->children.begin();

    if(it->value.id() == SGrammar::eID_seq_loc) {
        CRef<CSeq_loc> loc = x_seq_loc(it, context);
        delta->SetSeq().SetLoc(*loc);
    } else if(it->value.id() == SGrammar::eID_nuc_range || it->value.id() == SGrammar::eID_prot_range) {
        CRef<CVariantPlacement> p = x_range(it, context);
        if(p->IsSetStart_offset() || p->IsSetStop_offset()) {
            HGVS_THROW(eSemantic, "Intronic loc is not supported in this context");
        }
        delta->SetSeq().SetLoc().Assign(p->GetLoc());
    } else if(it->value.id() == SGrammar::eID_raw_seq_or_len) {
        CRef<CSeq_literal> literal = x_raw_seq_or_len(it, context);
        delta->SetSeq().SetLiteral(*literal);
    } else {
        HGVS_ASSERT_RULE(it, eID_NONE);
    }

    return delta;
}


bool CHgvsParser::s_hgvsaa2ncbieaa(const string& hgvsaa, string& out)
{
    //try to interpret sequence that was matched by either of aminoacid1, aminoacid, or aminoacid3
    string tmp_out(""); //so that the caller may pass same variable for in and out
    bool ret = s_hgvsaa2ncbieaa(hgvsaa, true, tmp_out);
    if(!ret) {
        ret = s_hgvsaa2ncbieaa(hgvsaa, false, tmp_out);
    }
    if(!ret) {
        ret = s_hgvs_iupacaa2ncbieaa(hgvsaa, tmp_out);
    }

    out = tmp_out;
    return ret;
}

bool CHgvsParser::s_hgvs_iupacaa2ncbieaa(const string& hgvsaa, string& out)
{
    out = hgvsaa;

    //"X" used to mean "Ter" in HGVS; now it means "unknown aminoacid"
    //Still, we'll interpret it as Ter, simply beacuse it is more likely
    //that the submitter is using legacy representation.
    NStr::ReplaceInPlace(out, "X", "*");
    NStr::ReplaceInPlace(out, "?", "X");
    return true;
}

bool CHgvsParser::s_hgvsaa2ncbieaa(const string& hgvsaa, bool uplow, string& out)
{
    string in = hgvsaa;
    out = "";
    while(in != "") {
        bool found = false;
        for(size_t i_ncbistdaa = 0; i_ncbistdaa < 28; i_ncbistdaa++) {
            string iupac3 = CSeqportUtil::GetIupacaa3(i_ncbistdaa);
            if(NStr::StartsWith(in, uplow ? iupac3 : NStr::ToUpper(iupac3))) {
                size_t i_ncbieaa = CSeqportUtil::GetMapToIndex(CSeq_data::e_Ncbistdaa,
                                                               CSeq_data::e_Ncbieaa,
                                                               i_ncbistdaa);
                out += CSeqportUtil::GetCode(CSeq_data::e_Ncbieaa, i_ncbieaa);
                found = true;
                break;
            }
        }
        if(found) {
            in = in.substr(3);
        } else if(NStr::StartsWith(in, "*")) { out.push_back('*'); in = in.substr(1);
        } else if(NStr::StartsWith(in, "X")) { out.push_back('*'); in = in.substr(1);
        } else if(NStr::StartsWith(in, "?")) { out.push_back('X'); in = in.substr(1);
        //} else if(NStr::StartsWith(in, "STOP", NStr::eNocase)) { out.push_back('X'); in = in.substr(4); //VAR-283
        } else {
            out = hgvsaa;
            return false;
        }
    }
    return true;
}



CRef<CSeq_literal> CHgvsParser::x_raw_seq(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_raw_seq);
    TIterator it = i->children.begin();

    string seq_str(it->value.begin(), it->value.end());

    CRef<CSeq_literal>literal(new CSeq_literal);
    if(context.GetPlacement().GetMol() == CVariantPlacement::eMol_protein) {
        s_hgvsaa2ncbieaa(seq_str, seq_str);
        literal->SetSeq_data().SetNcbieaa().Set(seq_str);
    } else {
        seq_str = NStr::ToUpper(seq_str);
        NStr::ReplaceInPlace(seq_str, "U", "T");
        literal->SetSeq_data().SetIupacna().Set(seq_str);
    }

    literal->SetLength(seq_str.size());

    vector<TSeqPos> bad;
    CSeqportUtil::Validate(literal->GetSeq_data(), &bad);

    if(bad.size() > 0) {
        HGVS_THROW(eSemantic, "Invalid sequence at pos " +  NStr::IntToString(bad[0]) + " in " + seq_str);
    }

    return literal;
}


int GetDeltaLength(const CDelta_item& delta, int loc_len)
{
    int len = !delta.IsSetSeq()          ? 0 
            : delta.GetSeq().IsLiteral() ? delta.GetSeq().GetLiteral().GetLength()
            : delta.GetSeq().IsLoc()     ? sequence::GetLength(delta.GetSeq().GetLoc(), NULL)
            : delta.GetSeq().IsThis()    ? loc_len
            : 0;
   if(delta.IsSetMultiplier()) {
        len *= delta.GetMultiplier();
   }
   return len; 
}

CVariation_inst::EType GetDelInsSubtype(int del_len, int ins_len)
{
    return del_len > 0 && ins_len == 0        ? CVariation_inst::eType_del
         : del_len == 0 && ins_len > 0        ? CVariation_inst::eType_ins
         : del_len == ins_len && del_len != 1 ? CVariation_inst::eType_mnp
         : del_len == ins_len && del_len == 1 ? CVariation_inst::eType_snv
         :                                      CVariation_inst::eType_delins;
}

CRef<CVariation> CHgvsParser::x_delins(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_delins);
    TIterator it = i->children.begin();
    CRef<CVariation> del_vr = x_deletion(it, context);
    ++it;
    CRef<CVariation> ins_vr = x_insertion(it, context, false);
        //note: don't verify location, as it must be len=2 for pure-insertion only

    //The resulting delins variation has deletion's placement (with asserted seq, if any),
    //and insertion's inst, except action type is "replace" (default) rather than "ins-before",
    //so we reset action

    int placement_len = CVariationUtil::s_GetLength(SetFirstPlacement(*del_vr), NULL);
    int del_len = GetDeltaLength(*del_vr->GetData().GetInstance().GetDelta().front(), placement_len);
    int ins_len = GetDeltaLength(*ins_vr->GetData().GetInstance().GetDelta().front(), placement_len);
    del_vr->SetData().SetInstance().SetType(GetDelInsSubtype(del_len, ins_len));

    del_vr->SetData().SetInstance().SetDelta() = ins_vr->SetData().SetInstance().SetDelta();
    del_vr->SetData().SetInstance().SetDelta().front()->ResetAction();

    if(ins_len == 1 && del_len == 1) {
        CRef<CVariationException> ex(new CVariationException);
        ex->SetCode(CVariationException::eCode_hgvs_parsing);
        ex->SetMessage("delins used for single-nt substitution");
        SetFirstPlacement(*del_vr).SetExceptions().push_back(ex);
    }

    return del_vr;
}

CRef<CVariation> CHgvsParser::x_deletion(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_deletion);
    TIterator it = i->children.begin();
    CRef<CVariation> vr(new CVariation);
    CVariation_inst& var_inst = vr->SetData().SetInstance();

    var_inst.SetType(CVariation_inst::eType_del);
    SetFirstPlacement(*vr).Assign(context.GetPlacement());

    CRef<CDelta_item> di(new CDelta_item);
    di->SetAction(CDelta_item::eAction_del_at);
    di->SetSeq().SetThis();
    var_inst.SetDelta().push_back(di);

    ++it;

    if(it != i->children.end() && it->value.id() == SGrammar::eID_raw_seq_or_len) {
        CRef<CSeq_literal> literal = x_raw_seq_or_len(it, context);
        ++it;
        SetFirstPlacement(*vr).SetSeq(*literal);
    }

    var_inst.SetDelta();
    return vr;
}


CRef<CVariation> CHgvsParser::x_insertion(TIterator const& i, const CContext& context, bool check_loc)
{
    HGVS_ASSERT_RULE(i, eID_insertion);
    TIterator it = i->children.begin();
    ++it; //skip ins
    CRef<CVariation> vr(new CVariation);
    CVariation_inst& var_inst = vr->SetData().SetInstance();

    var_inst.SetType(CVariation_inst::eType_ins);

    SetFirstPlacement(*vr).Assign(context.GetPlacement());

    if(check_loc && CVariationUtil::s_GetLength(*vr->GetPlacements().front(), NULL) != 2) {
        HGVS_THROW(eSemantic, "Location must be a dinucleotide");
    }

    TDelta delta_ins = x_seq_ref(it, context);

    //todo:
    //alternative representation: if delta is literal, might use action=morph and prefix/suffix the insertion with the flanking nucleotides.
    delta_ins->SetAction(CDelta_item::eAction_ins_before);

    var_inst.SetDelta().push_back(delta_ins);

    return vr;
}


CRef<CVariation> CHgvsParser::x_duplication(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_duplication);
    TIterator it = i->children.begin();
    CRef<CVariation> vr(new CVariation);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_ins); //replace seq @ location with this*2

    SetFirstPlacement(*vr).Assign(context.GetPlacement());

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetThis(); //delta->SetSeq().SetLoc(vr->SetLocation());
    delta->SetMultiplier(2);
    var_inst.SetDelta().push_back(delta);

    ++it; //skip dup

    //the next node is either expected length or expected sequence
    if(it != i->children.end() && it->value.id() == SGrammar::eID_seq_ref) {
        TDelta dup_seq = x_seq_ref(it, context);
        if(dup_seq->GetSeq().IsLiteral()) {
            SetFirstPlacement(*vr).SetSeq(dup_seq->SetSeq().SetLiteral());

            if(CVariationUtil::s_GetLength(*vr->GetPlacements().front(), NULL) != dup_seq->GetSeq().GetLiteral().GetLength()) {
                HGVS_THROW(eSemantic, "Location length and asserted sequence length differ");
            }
        }
    }

    return vr;
}


CRef<CVariation> CHgvsParser::x_no_change(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_no_change);
    TIterator it = i->children.begin();
    CRef<CVariation> vr(new CVariation);
    CVariation_inst& var_inst = vr->SetData().SetInstance();

    CVariantPlacement& p = SetFirstPlacement(*vr);
    p.Assign(context.GetPlacement());

    //VAR-574: The no-change variation is interpreted as X>X

    if(it->value.id() == SGrammar::eID_raw_seq) {
        CRef<CSeq_literal> seq_from = x_raw_seq(it, context);
        p.SetSeq(*seq_from);
        ++it;
    } else {
        CVariationUtil util(context.GetScope());
        util.AttachSeq(p);
        if(p.IsSetExceptions()) {
            HGVS_THROW(eSemantic, "Can't get sequence at location, and no asserted sequence specified in the expression");
        }
    }

    //var_inst.SetType(CVariation_inst::eType_identity);
    var_inst.SetType(p.GetSeq().GetLength() == 1 ? CVariation_inst::eType_snv : CVariation_inst::eType_mnp);

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLiteral().Assign(p.GetSeq());
    var_inst.SetDelta().push_back(delta);

    return vr;
}


CRef<CVariation> CHgvsParser::x_nuc_subst(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_nuc_subst);
    TIterator it = i->children.begin();
    CRef<CVariation> vr(new CVariation);
    CVariation_inst& var_inst = vr->SetData().SetInstance();

    SetFirstPlacement(*vr).Assign(context.GetPlacement());

    if(it->value.id() == SGrammar::eID_raw_seq) {
        CRef<CSeq_literal> seq_from = x_raw_seq(it, context);
        SetFirstPlacement(*vr).SetSeq(*seq_from);
        ++it;
    }

    ++it;//skip ">"

    CRef<CSeq_literal> seq_to = x_raw_seq_or_len(it, context);
    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLiteral(*seq_to);
    var_inst.SetDelta().push_back(delta);

    var_inst.SetType(GetDelInsSubtype(CVariationUtil::s_GetLength(SetFirstPlacement(*vr), NULL), seq_to->GetLength()));

    return vr;
}


CRef<CVariation> CHgvsParser::x_nuc_inv(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_nuc_inv);

    TIterator it = i->children.begin();
    CRef<CVariation> vr(new CVariation);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_inv);

    SetFirstPlacement(*vr).Assign(context.GetPlacement());

#if 0
    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLoc().Assign(*loc);
    delta->SetSeq().SetLoc().FlipStrand();
    var_inst.SetDelta().push_back(delta);
#else
    //don't put anything in the delta, as the inversion sequence is placement-specific, not variation-specific
    var_inst.SetDelta(); 
#endif

    ++it;

     //capture asserted seq
     if(it != i->children.end() && it->value.id() == SGrammar::eID_seq_ref) {
         TDelta dup_seq = x_seq_ref(it, context);
         if(dup_seq->GetSeq().IsLiteral()) {
             SetFirstPlacement(*vr).SetSeq(dup_seq->SetSeq().SetLiteral());
         }
     }

    return vr;
}


CRef<CVariation> CHgvsParser::x_ssr(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_ssr);
    TIterator it = i->children.begin();
    CRef<CVariation> vr(new CVariation);
    vr->SetData().SetInstance().SetType(CVariation_inst::eType_microsatellite);


    CRef<CSeq_literal> literal;
    if(it->value.id() == SGrammar::eID_raw_seq) {
        literal = x_raw_seq(it, context);
        ++it;
    }



    SetFirstPlacement(*vr).Assign(context.GetPlacement());

#if 1
    if(!literal.IsNull() && literal->IsSetSeq_data() && literal->GetSeq_data().IsIupacna()) {
        CRef<CSeq_loc> ssr_loc = FindSSRLoc(SetFirstPlacement(*vr).GetLoc(), literal->GetSeq_data().GetIupacna(), context.GetScope());
        SetFirstPlacement(*vr).SetLoc().Assign(*ssr_loc);
    }
#else
    if(SetFirstPlacement(*vr).GetLoc().IsPnt() && !literal.IsNull()) {
        //The location may either specify a repeat unit, or point to the first base of a repeat unit.
        //We normalize it so it is alwas the repeat unit.
        ExtendDownstream(SetFirstPlacement(*vr), literal->GetLength() - 1);
    }
#endif


    if(it->value.id() == SGrammar::eID_ssr) { // list('['>>int_p>>']', '+') with '[',']','+' nodes discarded;
        //Note: see ssr grammar in the header for reasons why we have to match all alleles here
        //rather than match them separately as mut_insts

        vr->SetData().SetSet().SetType(CVariation::TData::TSet::eData_set_type_genotype);
        for(; it != i->children.end(); ++it) {
            string s1(it->value.begin(), it->value.end());
            CRef<CVariation> vr2(new CVariation);
            vr2->SetData().SetInstance().SetType(CVariation_inst::eType_microsatellite);

            TDelta delta(new TDelta::TObjectType);
            if(!literal.IsNull()) {
                delta->SetSeq().SetLiteral().Assign(*literal);
            } else {
                delta->SetSeq().SetThis();
            }
            delta->SetMultiplier(NStr::StringToInt(s1));

            vr2->SetData().SetInstance().SetDelta().push_back(delta);
            vr->SetData().SetSet().SetVariations().push_back(vr2);
        }
        vr = x_unwrap_iff_singleton(*vr);
    } else {
        TDelta delta(new TDelta::TObjectType);
        if(!literal.IsNull()) {
            delta->SetSeq().SetLiteral().Assign(*literal);
        } else {
            delta->SetSeq().SetThis();
        }

        SFuzzyInt int_fuzz = x_int_fuzz(it, context);
        delta->SetMultiplier(int_fuzz.value);
        if(int_fuzz.fuzz.IsNull()) {
            ;
        } else {
            delta->SetMultiplier_fuzz(*int_fuzz.fuzz);
        }
        vr->SetData().SetInstance().SetDelta().push_back(delta);
    }

    return vr;
}


CRef<CVariation> CHgvsParser::x_translocation(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_translocation);
    TIterator it = i->children.end() - 1; //note: seq-loc follows iscn expression, i.e. last child
    CRef<CVariation> vr(new CVariation);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_translocation);

    CRef<CSeq_loc> loc = x_seq_loc(it, context);
    SetFirstPlacement(*vr).SetLoc().Assign(*loc);
    CVariationUtil util(context.GetScope());
    SetFirstPlacement(*vr).SetMol(util.GetMolType(sequence::GetId(*loc, NULL)));

    it = i->children.begin();
    string iscn_expr(it->value.begin(), it->value.end());
    vr->SetSynonyms().push_back("ISCN:" + iscn_expr);
    var_inst.SetDelta(); //no delta contents

    return vr;
}


CRef<CVariation> CHgvsParser::x_conversion(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_conversion);
    TIterator it = i->children.begin();
    CRef<CVariation> vr(new CVariation);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_transposon);

    SetFirstPlacement(*vr).Assign(context.GetPlacement());

    ++it;
    CRef<CSeq_loc> loc_other = x_seq_loc(it, context);

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLoc().Assign(*loc_other);
    var_inst.SetDelta().push_back(delta);

    return vr;
}


CRef<CVariation> CHgvsParser::x_prot_fs(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_prot_fs);
    TIterator it = i->children.begin();
    CRef<CVariation> vr(new CVariation);

    if(context.GetPlacement().GetMol() != CVariantPlacement::eMol_protein) {
        HGVS_THROW(eContext, "Frameshift can only be specified in protein context");
    }

    vr->SetData().SetNote("Frameshift");
    vr->SetFrameshift();

    SetFirstPlacement(*vr).Assign(context.GetPlacement());

    ++it; //skip 'fs'
    if(it != i->children.end()) {
        //fsX# description: the remaining tokens are 'X' and integer
        ++it; //skip 'X'
        if(it != i->children.end()) {
            string s(it->value.begin(), it->value.end());
            int x_length = NStr::StringToInt(s);
            vr->SetFrameshift().SetX_length(x_length);
        }
    }

    return vr;
}


CRef<CVariation> CHgvsParser::x_prot_ext(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_prot_ext);
    TIterator it = i->children.begin();

    if(context.GetPlacement().GetMol() != CVariantPlacement::eMol_protein) {
        HGVS_THROW(eContext, "Expected protein context");
    }

    CRef<CVariation> vr(new CVariation);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_prot_other);
    string ext_type_str(it->value.begin(), it->value.end());
    ++it;
    string ext_len_str(it->value.begin(), it->value.end());
    int ext_len = NStr::StringToInt(ext_len_str);

    SetFirstPlacement(*vr).Assign(context.GetPlacement());
    SetFirstPlacement(*vr).SetLoc().SetPnt().SetId().Assign(context.GetId());
    SetFirstPlacement(*vr).SetLoc().SetPnt().SetStrand(eNa_strand_plus);

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLiteral().SetLength(abs(ext_len) + 1);
        //extension of Met or X by N bases = replacing first or last AA with (N+1) AAs

    if(ext_type_str == "extMet") {
        if(ext_len > 0) {
            HGVS_THROW(eSemantic, "extMet must be followed by a negative integer");
        }
        SetFirstPlacement(*vr).SetLoc().SetPnt().SetPoint(0);
        //extension precedes first AA
        var_inst.SetDelta().push_back(delta);
    } else if(ext_type_str == "extX" || ext_type_str == "ext*") {
        if(ext_len < 0) {
            HGVS_THROW(eSemantic, "exX must be followed by a non-negative integer");
        }

        SetFirstPlacement(*vr).SetLoc().SetPnt().SetPoint(context.GetBioseqHandle().GetInst_Length() - 1);
        //extension follows last AA
        var_inst.SetDelta().push_back(delta);
    } else {
        HGVS_THROW(eGrammatic, "Unexpected ext_type: " + ext_type_str);
    }

    return vr;
}


CRef<CVariation> CHgvsParser::x_prot_missense(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_prot_missense);
    TIterator it = i->children.begin();

    CRef<CSeq_literal> prot_literal = x_raw_seq(it, context);

    if(context.GetPlacement().GetMol() != CVariantPlacement::eMol_protein) {
        HGVS_THROW(eContext, "Expected protein context");
    }

    CRef<CVariation> vr(new CVariation);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(prot_literal->GetLength() == 1 ? CVariation_inst::eType_prot_missense : CVariation_inst::eType_prot_other);

    SetFirstPlacement(*vr).Assign(context.GetPlacement());

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLiteral(*prot_literal);
    var_inst.SetDelta().push_back(delta);

    return vr;
}


CRef<CVariation>  CHgvsParser::x_string_content(TIterator const& i, const CContext& context)
{
    CRef<CVariation> vr(new CVariation);
    string s(i->value.begin(), i->value.end());
    s = s.substr(1); //truncate the leading pipe
    SetFirstPlacement(*vr).Assign(context.GetPlacement());
    vr->SetData().SetNote(s);
    return vr;
}


CRef<CVariation> CHgvsParser::x_mut_inst(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_mut_inst);

    TIterator it = i->children.begin();

    CRef<CVariation> vr(new CVariation);
    if(it->value.id() == SGrammar::eID_mut_inst) {
        string s(it->value.begin(), it->value.end());
        if(s == "?") {
            vr->SetData().SetUnknown();
            SetFirstPlacement(*vr).Assign(context.GetPlacement());
        } else {
            vr = x_string_content(it, context);
        }
    } else {
        vr =
            it->value.id() == SGrammar::eID_no_change     ? x_no_change(it, context)
          : it->value.id() == SGrammar::eID_delins        ? x_delins(it, context)
          : it->value.id() == SGrammar::eID_deletion      ? x_deletion(it, context)
          : it->value.id() == SGrammar::eID_insertion     ? x_insertion(it, context, true)
          : it->value.id() == SGrammar::eID_duplication   ? x_duplication(it, context)
          : it->value.id() == SGrammar::eID_nuc_subst     ? x_nuc_subst(it, context)
          : it->value.id() == SGrammar::eID_nuc_inv       ? x_nuc_inv(it, context)
          : it->value.id() == SGrammar::eID_ssr           ? x_ssr(it, context)
          : it->value.id() == SGrammar::eID_conversion    ? x_conversion(it, context)
          : it->value.id() == SGrammar::eID_prot_ext      ? x_prot_ext(it, context)
          : it->value.id() == SGrammar::eID_prot_fs       ? x_prot_fs(it, context)
          : it->value.id() == SGrammar::eID_prot_missense ? x_prot_missense(it, context)
          : it->value.id() == SGrammar::eID_translocation ? x_translocation(it, context)
          : CRef<CVariation>(NULL);

        if(vr.IsNull()) {
            HGVS_ASSERT_RULE(it, eID_NONE);
        }
    }

    return vr;
}

CRef<CVariation> CHgvsParser::x_expr1(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_expr1);
    TIterator it = i->children.begin();
    CRef<CVariation> vr;

    string s(it->value.begin(), it->value.end());
    if(it->value.id() == i->value.id() && s == "(") {
        ++it;
        vr = x_expr1(it, context);
        SetComputational(*vr);
    } else if(it->value.id() == SGrammar::eID_list1a) {
        vr = x_list(it, context);
    } else if(it->value.id() == SGrammar::eID_header) {
        CContext local_ctx = x_header(it, context);
        ++it;
        vr = x_expr2(it, local_ctx);
    } else if(it->value.id() == SGrammar::eID_translocation) {
        vr = x_translocation(it, context);
    } else {
        HGVS_ASSERT_RULE(it, eID_NONE);
    }

    return vr;
}

CRef<CVariation> CHgvsParser::x_expr2(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_expr2);
    TIterator it = i->children.begin();
    CRef<CVariation> vr;

    string s(it->value.begin(), it->value.end());
    if(it->value.id() == i->value.id() && s == "(") {
        ++it;
        vr = x_expr2(it, context);
        SetComputational(*vr);
    } else if(it->value.id() == SGrammar::eID_list2a) {
        vr = x_list(it, context);
    } else if(it->value.id() == SGrammar::eID_location) {
        CContext local_context(context);
        CRef<CVariantPlacement> placement = x_location(it, local_context);
        local_context.SetPlacement().Assign(*placement);
        ++it;
        vr = x_expr3(it, local_context);
    } else if(it->value.id() == SGrammar::eID_prot_ext) {
        vr = x_prot_ext(it, context);
    } else if(it->value.id() == SGrammar::eID_no_change) {
        vr = x_no_change(it, context);
    } else if(it->value.id() == i->value.id()) {
        vr.Reset(new CVariation);
        SetFirstPlacement(*vr).Assign(context.GetPlacement());

        if(s == "?") {
            vr->SetData().SetUnknown();
            //SetFirstPlacement(*vr).SetLoc().SetEmpty().Assign(context.GetId());
        } else if(s == "0?" || s == "0") {
            //loss of product: represent as deletion of the whole product sequence.
            SetFirstPlacement(*vr).SetLoc().SetWhole().Assign(context.GetId());
            CVariation_inst& var_inst = vr->SetData().SetInstance();
            var_inst.SetType(CVariation_inst::eType_del);
            CRef<CDelta_item> di(new CDelta_item);
            di->SetAction(CDelta_item::eAction_del_at);
            di->SetSeq().SetThis();
            var_inst.SetDelta().push_back(di);

            if(s == "0?") {
                SetComputational(*vr);
            }
        } else {
            HGVS_THROW(eGrammatic, "Unexpected expr terminal: " + s);
        }
    } else {
        HGVS_ASSERT_RULE(it, eID_NONE);
    }

    return vr;
}


CRef<CVariation> CHgvsParser::x_expr3(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_expr3);
    TIterator it = i->children.begin();
    CRef<CVariation> vr;

    string s(it->value.begin(), it->value.end());
    if(it->value.id() == i->value.id() && s == "(") {
        ++it;
        vr = x_expr3(it, context);
        SetComputational(*vr);
    } else if(it->value.id() == SGrammar::eID_list3a) {
        vr = x_list(it, context);
    } else if(it->value.id() == SGrammar::eID_mut_inst) {
        vr.Reset(new CVariation);
        vr->SetData().SetSet().SetType(CVariation::TData::TSet::eData_set_type_compound);
        for(; it != i->children.end(); ++it) {
            CRef<CVariation> inst_ref = x_mut_inst(it, context);

            if(inst_ref->GetData().IsNote()
              && inst_ref->GetData().GetNote() == "Frameshift"
              && vr->SetData().SetSet().SetVariations().size() > 0)
            {
                //if inst_ref is a frameshift subexpression, we need to attach it as attribute of the
                //previous variation in a compound inst-list, since frameshift is not a subtype of
                //Variation.data, and thus not represented as a separate subvariation.

                vr->SetData().SetSet().SetVariations().back()->SetFrameshift().Assign(inst_ref->GetFrameshift());
            } else {
                vr->SetData().SetSet().SetVariations().push_back(inst_ref);
            }
        }
        vr = x_unwrap_iff_singleton(*vr);
    } else {
        HGVS_ASSERT_RULE(it, eID_NONE);
    }

    return vr;
}


CVariation::TData::TSet::EData_set_type CHgvsParser::x_list_delimiter(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_list_delimiter);
    TIterator it = i->children.begin();
    string s(it->value.begin(), it->value.end());

    return s == "//" ? CVariation::TData::TSet::eData_set_type_other //chimeric - not represented in the schema
         : s == "/"  ? CVariation::TData::TSet::eData_set_type_mosaic
         : s == "+"  ? CVariation::TData::TSet::eData_set_type_genotype
         : s == ","  ? CVariation::TData::TSet::eData_set_type_products
         : s == ";"  ? CVariation::TData::TSet::eData_set_type_haplotype //note that within context of list#a, ";" delimits genotype
         : s == "(+)" || s == "(;)" ? CVariation::TData::TSet::eData_set_type_individual
         : CVariation::TData::TSet::eData_set_type_unknown;
}


CRef<CVariation> CHgvsParser::x_list(TIterator const& i, const CContext& context)
{
    if(!SGrammar::s_is_list(i->value.id())) {
        HGVS_ASSERT_RULE(i, eID_NONE);
    }

    CRef<CVariation> vr(new CVariation);
    TVariationSet& varset = vr->SetData().SetSet();
    varset.SetType(CVariation::TData::TSet::eData_set_type_unknown);


    for(TIterator it = i->children.begin(); it != i->children.end(); ++it) {
        //will process two elements from the children list: delimiter and following expression.
        //The first one does not have the delimiter. The delimiter determines the set-type.
        if(it != i->children.begin()) {
            if(SGrammar::s_is_list_a(i->value.id())) {
                /*
                 * list#a is delimited by either ";"(HGVS-2.0) or "+"(HGVS_1.0);
                 * Both represent alleles within genotype.
                 * Note: the delimiter rule in the context is chset_p<>("+;"), i.e.
                 * a terminal, not a rule like list_delimiter; so calling
                 * x_list_delimiter(...) parser here would throw
                 */
                varset.SetType(CVariation::TData::TSet::eData_set_type_genotype);
            } else {
                CVariation::TData::TSet::EData_set_type set_type = x_list_delimiter(it, context);
                if(varset.GetType() == CVariation::TData::TSet::eData_set_type_unknown) {
                    varset.SetType(set_type);
                } else if(set_type != varset.GetType()) {
                    HGVS_THROW(eSemantic, "Non-unique delimiters within a list");
                }
            }
            ++it;
        } 

        CRef<CVariation> vr;
        if(it->value.id() == SGrammar::eID_expr1) {
            vr = x_expr1(it, context);
        } else if(it->value.id() == SGrammar::eID_expr2) {
            vr = x_expr2(it, context);
        } else if(it->value.id() == SGrammar::eID_expr3) {
            vr = x_expr3(it, context);
        } else if(SGrammar::s_is_list(it->value.id())) {
            vr = x_list(it, context);
        } else {
            HGVS_ASSERT_RULE(it, eID_NONE);
        }

        varset.SetVariations().push_back(vr);
    }

    vr = x_unwrap_iff_singleton(*vr);
    return vr;
}


CRef<CVariation> CHgvsParser::x_root(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_root);

    CRef<CVariation> vr = x_list(i, context);

    RepackageAssertedSequence(*vr);
    AdjustMoltype(*vr, context.GetScope());
    CVariationUtil::s_FactorOutPlacements(*vr);
    
    vr->Index();
    return vr;
}

CRef<CVariation>  CHgvsParser::x_unwrap_iff_singleton(CVariation& v)
{
    if(v.GetData().IsSet() && v.GetData().GetSet().GetVariations().size() == 1) {
        CRef<CVariation> first = v.SetData().SetSet().SetVariations().front();
        if(!first->IsSetPlacements() && v.IsSetPlacements()) {
            first->SetPlacements() = v.SetPlacements();
        }
        return first;
    } else {
        return CRef<CVariation>(&v);
    }
}


void CHgvsParser::sx_AppendMoltypeExceptions(CVariation& v, CScope& scope)
{
    CVariationUtil util(scope);
    for(CTypeIterator<CVariantPlacement> it(Begin(v)); it; ++it) {
        CVariantPlacement& p = *it;
        CVariantPlacement::TMol mol = util.GetMolType(sequence::GetId(p.GetLoc(), NULL));
        if(p.GetMol() != mol) {
            CRef<CVariantPlacement> p2(new CVariantPlacement);
            p2->Assign(p);
            p2->SetMol(mol);

            string asserted_header = CHgvsParser::s_SeqIdToHgvsStr(p, &scope);
            string expected_header = CHgvsParser::s_SeqIdToHgvsStr(*p2, &scope);

            CRef<CVariationException> ex(new CVariationException);
            ex->SetCode(CVariationException::eCode_inconsistent_asserted_moltype);
            ex->SetMessage("Inconsistent mol-type. asserted:'" + asserted_header + "'; expected:'" + expected_header + "'");
            p.SetExceptions().push_back(ex);
        }
    }
}

CRef<CVariation> CHgvsParser::AsVariation(const string& hgvs, TOpFlags flags)
{
    string hgvs2 = NStr::TruncateSpaces(hgvs);
    tree_parse_info<> info = pt_parse(hgvs2.c_str(), *s_grammar, +space_p);
    CRef<CVariation> vr;

    if(!info.full) {
#if 0
        CNcbiOstrstream ostr;
        tree_to_xml(ostr, info.trees, hgvs2.c_str() , CHgvsParser::SGrammar::s_GetRuleNames());
        string tree_str = CNcbiOstrstreamToString(ostr);
#endif
        HGVS_THROW(eGrammatic, "Syntax error at pos " + NStr::SizetToString(info.length + 1) + " in " + hgvs2 + "");
    } else {
        CContext context(m_scope, m_seq_id_resolvers, hgvs);
        vr = x_root(info.trees.begin(), context);
        vr->SetName(hgvs2);
        sx_AppendMoltypeExceptions(*vr, context.GetScope());

        CVariationUtil util(context.GetScope());
        util.CheckAmbiguitiesInLiterals(*vr);
    }

    return vr;
}



void CHgvsParser::AttachHgvs(CVariation& v)
{
    v.Index();

    //compute and attach placement-specific HGVS expressions
    for(CTypeIterator<CVariation> it(Begin(v)); it; ++it) {
        CVariation& v2 = *it;
        if(!v2.IsSetPlacements()) {
            continue;
        }
        NON_CONST_ITERATE(CVariation::TPlacements, it2, v2.SetPlacements()) {
            CVariantPlacement& p2 = **it2;

            if(!p2.GetLoc().GetId()) {
                continue;
            }

            if(p2.GetMol() != CVariantPlacement::eMol_protein && v2.GetConsequenceParent()) {
                //if this variation is in consequnece, only compute HGVS for protein variations
                //(as otherwise it will throw - can't have HGVS expression for protein with nuc placement)
                continue;
            }

            //compute hgvs-expression specific to the placement and the variation to which it is attached
            try {
                string hgvs_expression = AsHgvsExpression(v2, CConstRef<CSeq_id>(p2.GetLoc().GetId()));
                p2.SetHgvs_name(hgvs_expression);
            } catch (CException& e ) {
                CNcbiOstrstream ostr;
                ostr << MSerial_AsnText << p2;
                string s = CNcbiOstrstreamToString(ostr);
                NCBI_REPORT_EXCEPTION("Can't compute HGVS expression for " + s, e);
            }
        }
    }

    //If the root variation does not have placements (e.g. a container for placement-specific subvariations)
    //then compute the hgvs expression for the root placement and attach it to variation itself as a synonym.
    if(!v.IsSetPlacements()) {
        string root_output_hgvs = AsHgvsExpression(v);
        v.SetSynonyms().push_back(root_output_hgvs);
    }
}



};

END_NCBI_SCOPE

