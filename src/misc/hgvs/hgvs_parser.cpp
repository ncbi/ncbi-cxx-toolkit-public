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
#include <objects/general/User_object.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>

#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <misc/hgvs/hgvs_parser.hpp>
#include <misc/hgvs/variation_util.hpp>


BEGIN_NCBI_SCOPE

namespace variation_ref {


#define HGVS_THROW(err_code, message) NCBI_THROW(CHgvsParser::CHgvsParserException, err_code, message)

#define HGVS_ASSERT_RULE(i, rule_id) \
    if((i->value.id()) != (SGrammar::rule_id))             \
    {HGVS_THROW(eGrammatic, "Unexpected rule " + CHgvsParser::SGrammar::s_GetRuleName(i->value.id()) ); }


CHgvsParser::SGrammar::TRuleNames CHgvsParser::SGrammar::s_rule_names;
CHgvsParser::SGrammar CHgvsParser::s_grammar;


//attach asserted sequence to the variation-ref in a user-object. This is for
//internal representation only, as the variation-ref travels up through the
//parse-tree nodes. Before we return the final variation, this will be repackaged
//as set of variations (see RepackageAssertedSequence()). This is done so that
//we don't have to deal with possibility of a variation-ref being a package
//within the parser.
void AttachAssertedSequence(CVariation_ref& vr, const CSeq_literal& literal)
{
    CRef<CUser_object> uo(new CUser_object);
    uo->SetType().SetStr("hgvs_asserted_seq");

    uo->SetField("length").SetData().SetInt(literal.GetLength());
    if(literal.GetSeq_data().IsIupacna()) {
        uo->AddField("iupacna", literal.GetSeq_data().GetIupacna());
    } else if(literal.GetSeq_data().IsNcbieaa()) {
        uo->AddField("ncbieaa", literal.GetSeq_data().GetNcbieaa());
    } else {
        HGVS_THROW(eLogic, "Seq-data is neither IUPAC-AA or IUPAC-NA");
    }
    vr.SetExt(*uo);
}

//if a variation has an asserted sequence, repackage it as a set having
//the original variation and a synthetic one representing the asserted sequence
void RepackageAssertedSequence(CVariation_ref& vr)
{
    if(vr.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, vr.SetData().SetSet().SetVariations()) {
            RepackageAssertedSequence(**it);
        }
    } else {
        CRef<CVariation_ref> orig(new CVariation_ref);
        orig->Assign(vr);
        orig->ResetLocation(); //location will be set on the package, as it is the same for both members

        vr.SetData().SetSet().SetType(CVariation_ref::TData::TSet::eData_set_type_package);
        vr.SetData().SetSet().SetVariations().push_back(orig);
        vr.ResetExt();

        if(orig->IsSetExt() && orig->GetExt().GetType().GetStr() == "hgvs_asserted_seq") {
            CRef<CVariation_ref> asserted_vr(new CVariation_ref);
            vr.SetData().SetSet().SetVariations().push_back(asserted_vr);

            asserted_vr->SetData().SetInstance().SetObservation(CVariation_inst::eObservation_asserted);
            asserted_vr->SetData().SetInstance().SetType(CVariation_inst::eType_identity);

            CRef<CDelta_item> delta(new CDelta_item);
            delta->SetSeq().SetLiteral().SetLength(orig->GetExt().GetField("length").GetData().GetInt());
            if(orig->GetExt().HasField("iupacna")) {
                delta->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(orig->GetExt().GetField("iupacna").GetData().GetStr());
            } else {
                delta->SetSeq().SetLiteral().SetSeq_data().SetNcbieaa().Set(orig->GetExt().GetField("ncbieaa").GetData().GetStr());
            }

            if(   orig->GetData().GetInstance().GetDelta().size() > 0
               && orig->GetData().GetInstance().GetDelta().front()->IsSetAction()
               && orig->GetData().GetInstance().GetDelta().front()->GetAction() == CDelta_item::eAction_offset)
            {
                CRef<CDelta_item> offset_di(new CDelta_item);
                offset_di->Assign(*orig->GetData().GetInstance().GetDelta().front());
                asserted_vr->SetData().SetInstance().SetDelta().push_back(offset_di);
            }

            asserted_vr->SetData().SetInstance().SetDelta().push_back(delta);

            if(   orig->GetData().GetInstance().GetDelta().size() > 0
               && orig->GetData().GetInstance().GetDelta().back() != orig->GetData().GetInstance().GetDelta().front()
               && orig->GetData().GetInstance().GetDelta().back()->IsSetAction()
               && orig->GetData().GetInstance().GetDelta().back()->GetAction() == CDelta_item::eAction_offset)
            {
                CRef<CDelta_item> offset_di(new CDelta_item);
                offset_di->Assign(*orig->GetData().GetInstance().GetDelta().back());
                asserted_vr->SetData().SetInstance().SetDelta().push_back(offset_di);
            }
            orig->ResetExt();
        }
    }
}




TSeqPos CHgvsParser::SOffsetLoc::GetLength() const
{
    return ncbi::sequence::GetLength(*loc, NULL) + stop_offset.value - start_offset.value;
}

const CSeq_loc& CHgvsParser::CContext::GetLoc() const
{
    if(m_loc.loc.IsNull()) {
        HGVS_THROW(eContext, "No seq-loc in context");
    }
    return *m_loc.loc;
}

const CHgvsParser::SOffsetLoc& CHgvsParser::CContext::GetOffsetLoc() const
{
    return m_loc;
}

const CSeq_id& CHgvsParser::CContext::GetId() const
{
    if(m_seq_id.IsNull()) {
        HGVS_THROW(eContext, "No seq-id in context");
    }
    return *m_seq_id;
}

const CSeq_feat& CHgvsParser::CContext::GetCDS() const
{
    if(m_cds.IsNull()) {
        HGVS_THROW(eContext, "No CDS feature in context");
    }
    return *m_cds;
}

CHgvsParser::CContext::EMolType CHgvsParser::CContext::GetMolType(bool check) const
{
    if(check && m_mol_type == eMol_not_set) {
        HGVS_THROW(eContext, "No sequence in context");
    }
    return m_mol_type;
}

void CHgvsParser::CContext::SetId(const CSeq_id& id, EMolType mol_type)
{
    Clear();

    m_mol_type = mol_type;
    if(m_seq_id.IsNull()) {
        m_seq_id.Reset(new CSeq_id);
    }
    m_seq_id->Assign(id);

    m_bsh = m_scope->GetBioseqHandle(*m_seq_id);

    if(!m_bsh) {
        HGVS_THROW(eContext, "Cannnot get bioseq for seq-id " + id.AsFastaString());
    }

    if(mol_type == eMol_c) {
        for(CFeat_CI ci(m_bsh); ci; ++ci) {
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

void CHgvsParser::CContext::Validate(const CSeq_literal& literal, const CSeq_loc& loc) const
{
    //if literal has no sequence data - simply validate lengths
    if(!literal.IsSetSeq_data()) {
        if(literal.GetLength() != loc.GetTotalRange().GetLength()) {
            HGVS_THROW(eSemantic, "Literal length does not match location length");
        }
    }

    string seq1 = "";
    string seq2 = "";

    if(literal.GetSeq_data().IsIupacna()) {
        seq1 = literal.GetSeq_data().GetIupacna();
        CSeqVector v(loc, *m_scope, CBioseq_Handle::eCoding_Iupac);
        v.GetSeqData(v.begin(), v.end(), seq2);
    } else if(literal.GetSeq_data().IsNcbieaa()) {
        seq1 = literal.GetSeq_data().GetNcbieaa();
        CSeqVector v(loc, *m_scope, CBioseq_Handle::eCoding_Iupac);
        v.GetSeqData(v.begin(), v.end(), seq2);
    } else {
        HGVS_THROW(eLogic, "Seq-literal of unsupported type");
    }

    if(seq1 != seq2 && seq2 != "") {
        HGVS_THROW(eSemantic, "Expected sequence '" + seq1 + "'; found '" + seq2 + "'");
    }
}

const string& CHgvsParser::SGrammar::s_GetRuleName(parser_id id)
{
    TRuleNames::const_iterator it = s_GetRuleNames().find(id);
    if(it == s_GetRuleNames().end()) {
        HGVS_THROW(eLogic, "Rule name not hardcoded");
    } else {
        return it->second;
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
    return fuzzy_int;
}

CRef<CSeq_point> CHgvsParser::x_abs_pos(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_abs_pos);
    TIterator it = i->children.begin();

    CRef<CSeq_point> pnt(new CSeq_point);
    pnt->SetId().Assign(context.GetId());
    pnt->SetStrand(eNa_strand_plus);
    TSignedSeqPos offset(0);

    bool is_relative_to_stop_codon = false;
    if(i->children.size() == 2) {
        is_relative_to_stop_codon = true;
        string s(it->value.begin(), it->value.end());
        if(s != "*") {
            HGVS_THROW(eGrammatic, "Expected literal '*'");
        }
        if(context.GetMolType() != CContext::eMol_c) {
            HGVS_THROW(eContext, "Expected 'c.' context for stop-codon-relative coordinate");
        }

        offset = context.GetCDS().GetLocation().GetStop(eExtreme_Biological);
        ++it;
    } else {
        if (context.GetMolType() == CContext::eMol_c) {
            //Note: in RNA coordinates (r.) the coordinates are absolute, like in genomic sequences,
            //  "The RNA sequence type uses only GenBank mRNA records. The value 1 is assigned to the first
            //  base in the record and from there all bases are counted normally."
            //so the cds-start offset applies only to "c." coordinates
            offset = context.GetCDS().GetLocation().GetStart(eExtreme_Biological);
        }
    }

    SFuzzyInt int_fuzz = x_int_fuzz(it, context);
    if(int_fuzz.value > 0 && !is_relative_to_stop_codon) {
        /* In HGVS:
         * the nucleotide 3' of the translation stop codon is *1, the next *2, etc.
         * # there is no nucleotide 0
         * # nucleotide 1 is the A of the ATG-translation initiation codon
         * # the nucleotide 5' of the ATG-translation initiation codon is -1, the previous -2, etc.
         * I.e. need to adjust if dealing with positive coordinates, except for *-relative ones.
         */
        offset--;
    }

    if(int_fuzz.fuzz.IsNull()) {
        pnt->SetPoint(offset + int_fuzz.value);
    } else {
        pnt->SetPoint(offset + int_fuzz.value);
        pnt->SetFuzz(*int_fuzz.fuzz);
        if(pnt->GetFuzz().IsRange()) {
            pnt->SetFuzz().SetRange().SetMin() += offset;
            pnt->SetFuzz().SetRange().SetMax() += offset;
        }
    }

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
        string s_sign(it->value.begin(), it->value.end());
        int sign1 = s_sign == "-" ? -1 : 1;
        ofpnt.offset.value *= sign1;
        if(ofpnt.offset.fuzz &&
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
    CContext::EMolType mol_type =
                       mol == "c" ? CContext::eMol_c
                     : mol == "g" ? CContext::eMol_g
                     : mol == "r" ? CContext::eMol_r
                     : mol == "p" ? CContext::eMol_p
                     : mol == "m" ? CContext::eMol_mt
                     : mol == "mt" ? CContext::eMol_mt
                     : CContext::eMol_not_set;

    it  = (i->children.rbegin() + 1)->children.begin();
    string id_str(it->value.begin(), it->value.end());

    CRef<CSeq_id> id(new CSeq_id(id_str));
    ctx.SetId(*id, mol_type);

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

    if(context.GetMolType() != CContext::eMol_p) {
        HGVS_THROW(eSemantic, "Expected protein context");
    }

    if(prot_literal->GetLength() != 1) {
        HGVS_THROW(eSemantic, "Expected single aa literal in prot-pos");
    }

    ++it;
    SOffsetPoint pnt = x_pos_spec(it, context);

    pnt.asserted_sequence = prot_literal->GetSeq_data().GetNcbieaa();

#if 0
    if(!pnt.IsOffset()) {
        //Create temporary loc and validate against it, since at this point context does not
        //have this loc set, since we are in the process of constructing it.
        CRef<CSeq_loc> tmp_loc(new CSeq_loc);
        tmp_loc->SetPnt(*pnt.pnt);
        context.Validate(*prot_literal, *tmp_loc);
    }
#endif

    return pnt;
}


CHgvsParser::SOffsetLoc CHgvsParser::x_range(TIterator const& i, const CContext& context)
{
    SOffsetPoint pnt1, pnt2;

    SOffsetLoc ofloc;
    ofloc.loc.Reset(new CSeq_loc(CSeq_loc::e_Int));

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

    ofloc.loc->SetInt().SetId(pnt1.pnt->SetId());
    ofloc.loc->SetInt().SetFrom(pnt1.pnt->GetPoint());
    ofloc.loc->SetInt().SetTo(pnt2.pnt->GetPoint());
    ofloc.loc->SetInt().SetStrand(pnt1.pnt->GetStrand());
    if(pnt1.pnt->IsSetFuzz()) {
        ofloc.loc->SetInt().SetFuzz_from(pnt1.pnt->SetFuzz());
    }

    if(pnt2.pnt->IsSetFuzz()) {
        ofloc.loc->SetInt().SetFuzz_to(pnt2.pnt->SetFuzz());
    }
    ofloc.start_offset = pnt1.offset;
    ofloc.stop_offset = pnt2.offset;

    if(pnt1.asserted_sequence != "" || pnt2.asserted_sequence != "") {
        ofloc.asserted_sequence = pnt1.asserted_sequence + ".." + pnt2.asserted_sequence;
    }

    return ofloc;
}

CHgvsParser::SOffsetLoc CHgvsParser::x_location(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_location);

    SOffsetLoc ofloc;
    ofloc.loc.Reset(new CSeq_loc);

    TIterator it = i->children.begin();
    CRef<CSeq_loc> loc(new CSeq_loc);
    if(it->value.id() == SGrammar::eID_prot_pos) {
        SOffsetPoint pnt = x_prot_pos(it, context);
        ofloc.loc->SetPnt(*pnt.pnt);
        ofloc.start_offset = pnt.offset;
        ofloc.asserted_sequence = pnt.asserted_sequence;
    } else if(it->value.id() == SGrammar::eID_pos_spec) {
        SOffsetPoint pnt = x_pos_spec(it, context);
        ofloc.loc->SetPnt(*pnt.pnt);
        ofloc.start_offset = pnt.offset;
        ofloc.asserted_sequence = pnt.asserted_sequence;
    } else if(it->value.id() == SGrammar::eID_nuc_range || it->value.id() == SGrammar::eID_prot_range) {
        ofloc = x_range(it, context);
    } else {
        HGVS_ASSERT_RULE(it, eID_NONE);
    }

    if(ofloc.loc->IsPnt() && ofloc.loc->GetPnt().GetPoint() == kInvalidSeqPos) {
        ofloc.loc->SetEmpty().Assign(context.GetId());
    }
    return ofloc;
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
    SOffsetLoc loc = x_location(it, local_context);

    if(flip_strand) {
        loc.loc->FlipStrand();
    }
    if(loc.start_offset.value || loc.stop_offset.value) {
        HGVS_THROW(eSemantic, "Intronic seq-locs are not supported in this context");
    }

    return loc.loc;
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
        SOffsetLoc ofloc = x_range(it, context);
        if(ofloc.IsOffset()) {
            HGVS_THROW(eSemantic, "Intronic loc is not supported in this context");
        }
        delta->SetSeq().SetLoc().Assign(*ofloc.loc);
    } else if(it->value.id() == SGrammar::eID_raw_seq) {
        CRef<CSeq_literal> raw_seq = x_raw_seq(it, context);
        delta->SetSeq().SetLiteral(*raw_seq);
    } else if(it->value.id() == SGrammar::eID_int_fuzz) {
        //known sequence length; may be approximate
        SFuzzyInt int_fuzz = x_int_fuzz(it, context);
        delta->SetSeq().SetLiteral().SetLength(int_fuzz.value);
        if(int_fuzz.fuzz.IsNull()) {
            ;//no-fuzz;
        } else if(int_fuzz.fuzz->IsLim() && int_fuzz.fuzz->GetLim() == CInt_fuzz::eLim_unk) {
            //unknown length (no value) - will represent as length=0 with gt fuzz
            delta->SetSeq().SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_gt);
        } else {
            delta->SetSeq().SetLiteral().SetFuzz(*int_fuzz.fuzz);
        }
    } else {
        HGVS_ASSERT_RULE(it, eID_NONE);
    }

    return delta;
}

string CHgvsParser::s_hgvsaa2ncbieaa(const string& hgvsaa)
{
    string ncbieaa = hgvsaa;
    NStr::ReplaceInPlace(ncbieaa, "Gly", "G");
    NStr::ReplaceInPlace(ncbieaa, "Pro", "P");
    NStr::ReplaceInPlace(ncbieaa, "Ala", "A");
    NStr::ReplaceInPlace(ncbieaa, "Val", "V");
    NStr::ReplaceInPlace(ncbieaa, "Leu", "L");
    NStr::ReplaceInPlace(ncbieaa, "Ile", "I");
    NStr::ReplaceInPlace(ncbieaa, "Met", "M");
    NStr::ReplaceInPlace(ncbieaa, "Cys", "C");
    NStr::ReplaceInPlace(ncbieaa, "Phe", "F");
    NStr::ReplaceInPlace(ncbieaa, "Tyr", "Y");
    NStr::ReplaceInPlace(ncbieaa, "Trp", "W");
    NStr::ReplaceInPlace(ncbieaa, "His", "H");
    NStr::ReplaceInPlace(ncbieaa, "Lys", "K");
    NStr::ReplaceInPlace(ncbieaa, "Arg", "R");
    NStr::ReplaceInPlace(ncbieaa, "Gln", "Q");
    NStr::ReplaceInPlace(ncbieaa, "Asn", "N");
    NStr::ReplaceInPlace(ncbieaa, "Glu", "E");
    NStr::ReplaceInPlace(ncbieaa, "Asp", "D");
    NStr::ReplaceInPlace(ncbieaa, "Ser", "S");
    NStr::ReplaceInPlace(ncbieaa, "Thr", "T");
    NStr::ReplaceInPlace(ncbieaa, "X", "*");
    NStr::ReplaceInPlace(ncbieaa, "?", "-");
    return ncbieaa;
}


string CHgvsParser::s_hgvsUCaa2hgvsUL(const string& hgvsaa)
{
    string s = hgvsaa;
    NStr::ReplaceInPlace(s, "GLY", "Gly");
    NStr::ReplaceInPlace(s, "PRO", "Pro");
    NStr::ReplaceInPlace(s, "ALA", "Ala");
    NStr::ReplaceInPlace(s, "VAL", "Val");
    NStr::ReplaceInPlace(s, "LEU", "Leu");
    NStr::ReplaceInPlace(s, "ILE", "Ile");
    NStr::ReplaceInPlace(s, "MET", "Met");
    NStr::ReplaceInPlace(s, "CYS", "Cys");
    NStr::ReplaceInPlace(s, "PHE", "Phe");
    NStr::ReplaceInPlace(s, "TYR", "Tyr");
    NStr::ReplaceInPlace(s, "TRP", "Trp");
    NStr::ReplaceInPlace(s, "HIS", "His");
    NStr::ReplaceInPlace(s, "LYS", "Lys");
    NStr::ReplaceInPlace(s, "ARG", "Arg");
    NStr::ReplaceInPlace(s, "GLN", "Gln");
    NStr::ReplaceInPlace(s, "ASN", "Asn");
    NStr::ReplaceInPlace(s, "GLU", "Glu");
    NStr::ReplaceInPlace(s, "ASP", "Asp");
    NStr::ReplaceInPlace(s, "SER", "Ser");
    NStr::ReplaceInPlace(s, "THR", "Thr");
    return s;
}


CRef<CSeq_literal> CHgvsParser::x_raw_seq(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_raw_seq);
    TIterator it = i->children.begin();
    string seq_str(it->value.begin(), it->value.end());

    CRef<CSeq_literal>literal(new CSeq_literal);
    if(context.GetMolType() == CContext::eMol_p) {
        seq_str = s_hgvsaa2ncbieaa(seq_str);
        literal->SetSeq_data().SetNcbieaa().Set(seq_str);
    } else {
        if(context.GetMolType() == CContext::eMol_r) {
            seq_str = NStr::ToUpper(seq_str);
            NStr::ReplaceInPlace(seq_str, "U", "T");
        }
        literal->SetSeq_data().SetIupacna().Set(seq_str);
    }

    literal->SetLength(seq_str.size());

    vector<TSeqPos> bad;
    CSeqportUtil::Validate(literal->GetSeq_data(), &bad);

    if(bad.size() > 0) {
        HGVS_THROW(eSemantic, "Invalid sequence at pos " +  NStr::NumericToString(bad[0]) + " in " + seq_str);
    }

    return literal;
}


CRef<CVariation_ref> CHgvsParser::x_delins(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_delins);
    TIterator it = i->children.begin();
    CRef<CVariation_ref> vr(new CVariation_ref);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_delins);
    vr->SetLocation().Assign(context.GetLoc());

    ++it; //skip "del"


    if(it->value.id() == SGrammar::eID_raw_seq) {
        CRef<CSeq_literal> literal = x_raw_seq(it, context);
        //context.Validate(*literal);
        ++it;
        AttachAssertedSequence(*vr, *literal);
    }

    ++it; //skip "ins"

    CRef<CDelta_item> di_del(new CDelta_item);
    di_del->SetAction(CDelta_item::eAction_del_at);
    di_del->SetSeq().SetThis();
    var_inst.SetDelta().push_back(di_del);

    TDelta di_ins = x_seq_ref(it, context);
    var_inst.SetDelta().push_back(di_ins);

    return vr;
}

CRef<CVariation_ref> CHgvsParser::x_deletion(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i,  eID_deletion);
    TIterator it = i->children.begin();
    CRef<CVariation_ref> vr(new CVariation_ref);
    CVariation_inst& var_inst = vr->SetData().SetInstance();

    var_inst.SetType(CVariation_inst::eType_del);
    vr->SetLocation().Assign(context.GetLoc());

    CRef<CDelta_item> di(new CDelta_item);
    di->SetAction(CDelta_item::eAction_del_at);
    di->SetSeq().SetThis();
    var_inst.SetDelta().push_back(di);

    ++it; //skip del

    if(it->value.id() == SGrammar::eID_raw_seq) {
        CRef<CSeq_literal> literal = x_raw_seq(it, context);
        //context.Validate(*literal);
        ++it;
        AttachAssertedSequence(*vr, *literal);
    }

    var_inst.SetDelta();
    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_insertion(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_insertion);
    TIterator it = i->children.begin();
    ++it; //skip ins
    CRef<CVariation_ref> vr(new CVariation_ref);
    CVariation_inst& var_inst = vr->SetData().SetInstance();

    var_inst.SetType(CVariation_inst::eType_ins);

    //point after which the insertion is.
    CRef<CSeq_loc> pnt_loc(new CSeq_loc);

    //verify that the HGVS-location is of length two, as in HGVS coordinates insertion
    //is denoted to be between the specified coordinates.
    TSeqPos len = context.GetOffsetLoc().GetLength();
    if(len != 2) {
        HGVS_THROW(eSemantic, "Encountered target location for an insertion with the length != 2");
    }

    pnt_loc->SetPnt().SetId().Assign(*context.GetLoc().GetId());
    pnt_loc->SetPnt().SetPoint(context.GetLoc().GetStop(eExtreme_Biological));
    pnt_loc->SetPnt().SetStrand(context.GetLoc().GetStrand());

    vr->SetLocation(*pnt_loc);

    //The delta consists of the self-location followed by the insertion sequence
    TDelta delta_ins = x_seq_ref(it, context);
    delta_ins->SetAction(CDelta_item::eAction_ins_before);

    var_inst.SetDelta().push_back(delta_ins);

    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_duplication(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_duplication);
    TIterator it = i->children.begin();
    CRef<CVariation_ref> vr(new CVariation_ref);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_ins);
    vr->SetLocation().Assign(context.GetLoc());

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetThis(); //delta->SetSeq().SetLoc(vr->SetLocation());
    delta->SetMultiplier(2);
    var_inst.SetDelta().push_back(delta);

    ++it; //skip dup

    //the next node is either expected length or expected sequence
    if(it != i->children.end() && it->value.id() == SGrammar::eID_seq_ref) {
        TDelta dup_seq = x_seq_ref(it, context);
        if(!dup_seq->GetSeq().IsLiteral()) {
            HGVS_THROW(eSemantic, "Expected literal after 'dup'");
        } else if(dup_seq->GetSeq().GetLiteral().GetLength() != context.GetOffsetLoc().GetLength()) {
            HGVS_THROW(eSemantic, "The expected duplication length is not equal to the location length");
        } else if(dup_seq->GetSeq().GetLiteral().IsSetSeq_data()) {
            //context.Validate(dup_seq->GetSeq().GetLiteral());
            AttachAssertedSequence(*vr, dup_seq->GetSeq().GetLiteral());
        }
    }

    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_nuc_subst(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_nuc_subst);
    TIterator it = i->children.begin();
    CRef<CVariation_ref> vr(new CVariation_ref);
    CVariation_inst& var_inst = vr->SetData().SetInstance();

    vr->SetLocation().Assign(context.GetLoc());
    var_inst.SetType(CVariation_inst::eType_snv);

    CRef<CSeq_literal> seq_from = x_raw_seq(it, context);
    if(seq_from->GetLength() != 1) {
        HGVS_THROW(eSemantic, "Expected literal of length 1 left of '>'");
    }

    //context.Validate(*seq_from);
    AttachAssertedSequence(*vr, *seq_from);

    ++it;//skip to ">"
    ++it;//skip to next
    CRef<CSeq_literal> seq_to = x_raw_seq(it, context);
    if(seq_to->GetLength() != 1) {
        HGVS_THROW(eSemantic, "Expected literal of length 1 right of '>'");
    }

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLiteral(*seq_to);
    var_inst.SetDelta().push_back(delta);

    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_nuc_inv(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_nuc_inv);

    TIterator it = i->children.begin();
    CRef<CVariation_ref> vr(new CVariation_ref);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_inv);

    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->Assign(context.GetLoc());
    vr->SetLocation(*loc);

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLoc().Assign(*loc);
    delta->SetSeq().SetLoc().FlipStrand();
    var_inst.SetDelta().push_back(delta);

    ++it;
    if(it != i->children.end()) {
        string len_str(it->value.begin(), it->value.end());
        TSeqPos len = NStr::StringToUInt(len_str);
        if(len != loc->GetTotalRange().GetLength()) {
            HGVS_THROW(eSemantic, "Inversion length not equal to location length");
        }
    }

    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_ssr(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_ssr);
    TIterator it = i->children.begin();
    CRef<CVariation_ref> vr(new CVariation_ref);
    vr->SetData().SetInstance().SetType(CVariation_inst::eType_microsatellite);


    CRef<CSeq_literal> literal;
    if(it->value.id() == SGrammar::eID_raw_seq) {
        literal = x_raw_seq(it, context);
        ++it;
    }


    //The location may either specify a repeat unit, or point to the first base
    //of a repeat unit. In terms of Variation-inst convention, the location
    //and delta should be the whole repeat-unit.
    CRef<CSeq_loc> loc(new CSeq_loc);
    if(context.GetLoc().IsPnt()) {
        loc->SetInt().SetId().Assign(*context.GetLoc().GetId());
        loc->SetInt().SetStrand(context.GetLoc().GetStrand());

        TSeqPos d = literal.IsNull() ? 0 : literal->GetLength() - 1;
        if(context.GetLoc().GetStrand() == eNa_strand_minus) {
            loc->SetInt().SetFrom(context.GetLoc().GetPnt().GetPoint() - d);
            loc->SetInt().SetTo(context.GetLoc().GetPnt().GetPoint());
        } else {
            loc->SetInt().SetFrom(context.GetLoc().GetPnt().GetPoint());
            loc->SetInt().SetTo(context.GetLoc().GetPnt().GetPoint() + d);
        }
    } else {
        loc->Assign(context.GetLoc());
        if(!literal.IsNull()) {
            //context.Validate(*literal);
            AttachAssertedSequence(*vr, *literal);
        }
    }

    vr->SetLocation().Assign(*loc);

    if(it->value.id() == SGrammar::eID_ssr) { // list('['>>int_p>>']', '+') with '[',']','+' nodes discarded;
        //Note: see ssr grammar in the header for reasons why we have to match all alleles here
        //rather than match them separately as mut_insts

        vr->SetData().SetSet().SetType(CVariation_ref::TData::TSet::eData_set_type_genotype);
        for(; it != i->children.end(); ++it) {
            string s1(it->value.begin(), it->value.end());
            CRef<CVariation_ref> vr2(new CVariation_ref);
            vr2->SetData().SetInstance().SetType(CVariation_inst::eType_microsatellite);

            TDelta delta(new TDelta::TObjectType);
            delta->SetSeq().SetLoc().Assign(*loc);
            delta->SetMultiplier(NStr::StringToInt(s1));

            vr2->SetData().SetInstance().SetDelta().push_back(delta);
            vr2->SetLocation().Assign(*loc);
            vr->SetData().SetSet().SetVariations().push_back(vr2);
        }
        vr = x_unwrap_iff_singleton(*vr);
    } else {
        TDelta delta(new TDelta::TObjectType);
        delta->SetSeq().SetLoc().Assign(*loc);

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


CRef<CVariation_ref> CHgvsParser::x_translocation(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_translocation);
    TIterator it = i->children.end() - 1; //note: seq-loc follows iscn expression, i.e. last child
    CRef<CVariation_ref> vr(new CVariation_ref);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_translocation);

    CRef<CSeq_loc> loc = x_seq_loc(it, context);
    vr->SetLocation(*loc);
    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLoc().SetNull();
    var_inst.SetDelta().push_back(delta);

    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_conversion(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_conversion);
    TIterator it = i->children.begin();
    CRef<CVariation_ref> vr(new CVariation_ref);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_transposon);
    vr->SetLocation().Assign(context.GetLoc());

    CRef<CSeq_loc> loc_this(new CSeq_loc);
    loc_this->Assign(context.GetLoc());

    ++it;
    CRef<CSeq_loc> loc_other = x_seq_loc(it, context);

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLoc().SetEquiv().Set().push_back(loc_this);
    delta->SetSeq().SetLoc().SetEquiv().Set().push_back(loc_other);
    var_inst.SetDelta().push_back(delta);

    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_prot_fs(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_prot_fs);
    TIterator it = i->children.begin();
    CRef<CVariation_ref> vr(new CVariation_ref);

    if(context.GetMolType() != context.eMol_p) {
        HGVS_THROW(eContext, "Frameshift can only be specified in protein context");
    }

    vr->SetData().SetNote("Frameshift");
    vr->SetLocation().Assign(context.GetLoc());

    typedef CVariation_ref::TConsequence::value_type::TObjectType TConsequence;
    CRef<TConsequence> cons(new TConsequence);
    cons->SetFrameshift();
    vr->SetConsequence().push_back(cons);


    ++it; //skip 'fs'
    if(it != i->children.end()) {
        //fsX# description: the remaining tokens are 'X' and integer
        ++it; //skip 'X'
        string s(it->value.begin(), it->value.end());
        int x_length = NStr::StringToInt(s);
        cons->SetFrameshift().SetX_length(x_length);
    }

    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_prot_ext(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_prot_ext);
    TIterator it = i->children.begin();

    if(context.GetMolType() != CContext::eMol_p) {
        HGVS_THROW(eContext, "Expected protein context");
    }

    CRef<CVariation_ref> vr(new CVariation_ref);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_ins);
    string ext_type_str(it->value.begin(), it->value.end());
    ++it;
    string ext_len_str(it->value.begin(), it->value.end());
    int ext_len = NStr::StringToInt(ext_len_str);

    vr->SetLocation().SetPnt().SetId().Assign(context.GetId());
    vr->SetLocation().SetPnt().SetStrand(eNa_strand_plus);

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLiteral().SetLength(abs(ext_len));

    TDelta delta_this(new TDelta::TObjectType);
    delta_this->SetSeq().SetThis();

    if(ext_type_str == "extMet") {
        if(ext_len > 0) {
            HGVS_THROW(eSemantic, "extMet must be followed by a negative integer");
        }
        vr->SetLocation().SetPnt().SetPoint(0);
        //extension precedes first AA
        var_inst.SetDelta().push_back(delta);
        var_inst.SetDelta().push_back(delta_this);
    } else if(ext_type_str == "extX") {
        if(ext_len < 0) {
            HGVS_THROW(eSemantic, "exX must be followed by a non-negative integer");
        }

        vr->SetLocation().SetPnt().SetPoint(context.GetLength() - 1);
        //extension follows last AA
        var_inst.SetDelta().push_back(delta_this);
        var_inst.SetDelta().push_back(delta);
    } else {
        HGVS_THROW(eGrammatic, "Unexpected ext_type: " + ext_type_str);
    }

    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_prot_missense(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_prot_missense);
    TIterator it = i->children.begin();

    HGVS_ASSERT_RULE(it, eID_aminoacid);
    TIterator it2 = it->children.begin();

    string seq_str(it2->value.begin(), it2->value.end());
    seq_str = s_hgvsaa2ncbieaa(seq_str);

    if(context.GetMolType() != CContext::eMol_p) {
        HGVS_THROW(eContext, "Expected protein context");
    }

    CRef<CVariation_ref> vr(new CVariation_ref);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_prot_missense);

    vr->SetLocation().Assign(context.GetLoc());

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetLiteral().SetSeq_data().SetNcbieaa().Set(seq_str);
    delta->SetSeq().SetLiteral().SetLength(1);
    var_inst.SetDelta().push_back(delta);

    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_identity(const CContext& context)
{
    CRef<CVariation_ref> vr(new CVariation_ref);
    CVariation_inst& var_inst = vr->SetData().SetInstance();
    var_inst.SetType(CVariation_inst::eType_identity);

    CRef<CSeq_loc> loc(new CSeq_loc);
    if(context.IsSetLoc()) {
        loc->Assign(context.GetLoc());
    } else {
        loc->SetEmpty().Assign(context.GetId());
    }

    vr->SetLocation(*loc);

    TDelta delta(new TDelta::TObjectType);
    delta->SetSeq().SetThis();
    var_inst.SetDelta().push_back(delta);

    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_mut_inst(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_mut_inst);

    TIterator it = i->children.begin();

    CRef<CVariation_ref> variation_ref(new CVariation_ref);
    if(it->value.id() == SGrammar::eID_mut_inst) {
        string s(it->value.begin(), it->value.end());
        if(s == "?") {
            variation_ref->SetData().SetUnknown();
            variation_ref->SetLocation().Assign(context.GetLoc());
        } else if(s == "=") {
            variation_ref = x_identity(context);
        } else {
            HGVS_THROW(eGrammatic, "Unexpected inst terminal: " + s);
        }
    } else {
        variation_ref =
            it->value.id() == SGrammar::eID_delins        ? x_delins(it, context)
          : it->value.id() == SGrammar::eID_deletion      ? x_deletion(it, context)
          : it->value.id() == SGrammar::eID_insertion     ? x_insertion(it, context)
          : it->value.id() == SGrammar::eID_duplication   ? x_duplication(it, context)
          : it->value.id() == SGrammar::eID_nuc_subst     ? x_nuc_subst(it, context)
          : it->value.id() == SGrammar::eID_nuc_inv       ? x_nuc_inv(it, context)
          : it->value.id() == SGrammar::eID_ssr           ? x_ssr(it, context)
          : it->value.id() == SGrammar::eID_conversion    ? x_conversion(it, context)
          : it->value.id() == SGrammar::eID_prot_ext      ? x_prot_ext(it, context)
          : it->value.id() == SGrammar::eID_prot_fs       ? x_prot_fs(it, context)
          : it->value.id() == SGrammar::eID_prot_missense ? x_prot_missense(it, context)
          : it->value.id() == SGrammar::eID_translocation ? x_translocation(it, context)
          : CRef<CVariation_ref>(NULL);

        if(variation_ref.IsNull()) {
            HGVS_ASSERT_RULE(it, eID_NONE);
        }
    }

    return variation_ref;
}

CRef<CVariation_ref> CHgvsParser::x_expr1(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_expr1);
    TIterator it = i->children.begin();
    CRef<CVariation_ref> vr;

    string s(it->value.begin(), it->value.end());
    if(it->value.id() == i->value.id() && s == "(") {
        ++it;
        vr = x_expr1(it, context);
        vr->SetValidated(false);
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

CRef<CVariation_ref> CHgvsParser::x_expr2(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_expr2);
    TIterator it = i->children.begin();
    CRef<CVariation_ref> vr;

    string s(it->value.begin(), it->value.end());
    if(it->value.id() == i->value.id() && s == "(") {
        ++it;
        vr = x_expr2(it, context);
        vr->SetValidated(false);
    } else if(it->value.id() == SGrammar::eID_list2a) {
        vr = x_list(it, context);
    } else if(it->value.id() == SGrammar::eID_location) {
        CContext local_context(context);
        SOffsetLoc ofloc = x_location(it, local_context);
        local_context.SetLoc(ofloc);
        ++it;
        vr = x_expr3(it, local_context);

        //if the location is intronic, create delta-items for intronic offsets
        CRef<CDelta_item> di1;
        if(ofloc.start_offset.value || ofloc.start_offset.fuzz) {
            di1.Reset(new CDelta_item);
            di1->SetAction(CDelta_item::eAction_offset);
            di1->SetSeq().SetLiteral().SetLength(abs(ofloc.start_offset.value));
            if(ofloc.start_offset.value < 0) {
                di1->SetMultiplier(-1);
            }

            if(ofloc.start_offset.fuzz) {
                di1->SetSeq().SetLiteral().SetFuzz().Assign(*ofloc.start_offset.fuzz);
            }
        }
        CRef<CDelta_item> di2;
        if(ofloc.stop_offset.value || ofloc.stop_offset.fuzz) {
            di2.Reset(new CDelta_item);
            di2->SetAction(CDelta_item::eAction_offset);
            if(ofloc.stop_offset.value < 0) {
                di2->SetMultiplier(-1);
            }

            di2->SetSeq().SetLiteral().SetLength(abs(ofloc.stop_offset.value));
            if(ofloc.stop_offset.fuzz) {
                di2->SetSeq().SetLiteral().SetFuzz().Assign(*ofloc.stop_offset.fuzz);
            }
        }

        //attach intronic offsets to the variation delta-items
        for(CTypeIterator<CVariation_inst> it2(Begin(*vr)); it2; ++it2) {
            CVariation_inst& inst = *it2;
            if(di1) {
                inst.SetDelta().push_front(di1);
            }
            if(di2) {
                inst.SetDelta().push_back(di2);
            }
        }


        //in some cases, e.g. protein variations, asserted sequence comes from the location-specification, rather than
        //variation-specification,
        if(ofloc.asserted_sequence != "") {
           CSeq_literal tmp_literal;
           tmp_literal.SetLength(ofloc.GetLength());
           if(context.GetMolType() == CContext::eMol_p) {
               tmp_literal.SetSeq_data().SetNcbieaa().Set(ofloc.asserted_sequence);
           } else {
               tmp_literal.SetSeq_data().SetIupacna().Set(ofloc.asserted_sequence);
           }
           AttachAssertedSequence(*vr, tmp_literal);
        }

    } else if(it->value.id() == SGrammar::eID_prot_ext) {
        vr = x_prot_ext(it, context);
    } else if(it->value.id() == i->value.id()) {
        vr.Reset(new CVariation_ref);
        if(s == "?") {
            vr->SetData().SetUnknown();
            vr->SetLocation().SetEmpty().Assign(context.GetId());
        } else if(s == "0?" || s == "0") {
            vr->SetData().SetUnknown();
            typedef CVariation_ref::TConsequence::value_type::TObjectType TConsequence;
            CRef<TConsequence> cons(new TConsequence);
            cons->SetNote("loss of product");
            vr->SetConsequence().push_back(cons);

            vr->SetLocation().SetEmpty().Assign(context.GetId());
            if(s == "0?") {
                vr->SetValidated(false);
            }
        } else if(s == "=") {
            vr = x_identity(context);
        } else {
            HGVS_THROW(eGrammatic, "Unexpected expr terminal: " + s);
        }
    } else {
        HGVS_ASSERT_RULE(it, eID_NONE);
    }

    return vr;
}


CRef<CVariation_ref> CHgvsParser::x_expr3(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_expr3);
    TIterator it = i->children.begin();
    CRef<CVariation_ref> vr;

    string s(it->value.begin(), it->value.end());
    if(it->value.id() == i->value.id() && s == "(") {
        ++it;
        vr = x_expr3(it, context);
        vr->SetValidated(false);
    } else if(it->value.id() == SGrammar::eID_list3a) {
        vr = x_list(it, context);
    } else if(it->value.id() == SGrammar::eID_mut_inst) {
        vr.Reset(new CVariation_ref);
        vr->SetData().SetSet().SetType(CVariation_ref::TData::TSet::eData_set_type_compound);
        for(; it != i->children.end(); ++it) {
            CRef<CVariation_ref> inst_ref = x_mut_inst(it, context);
            vr->SetData().SetSet().SetVariations().push_back(inst_ref);
        }
        vr = x_unwrap_iff_singleton(*vr);
    } else {
        HGVS_ASSERT_RULE(it, eID_NONE);
    }

    return vr;
}

CRef<CVariation_ref> CHgvsParser::x_list(TIterator const& i, const CContext& context)
{
    if(!SGrammar::s_is_list(i->value.id())) {
        HGVS_ASSERT_RULE(i, eID_NONE);
    }

    CRef<CVariation_ref> vr(new CVariation_ref);
    TVariationSet& varset = vr->SetData().SetSet();
    varset.SetType(CVariation_ref::TData::TSet::eData_set_type_unknown);
    string delimiter = "";

    for(TIterator it = i->children.begin(); it != i->children.end(); ++it) {
        //will process two elements from the children list: delimiter and following expression; the first one does not have the delimiter.
        if(it != i->children.begin()) {
            string delim(it->value.begin(), it->value.end());
            if(it->value.id() != i->value.id()) {
                HGVS_THROW(eGrammatic, "Expected terminal");
            } else if(delimiter == "") {
                //first time
                delimiter = delim;
            } else if(delimiter != delim) {
                HGVS_THROW(eSemantic, "Non-unique delimiters within a list");
            }
            ++it;
        } 

        CRef<CVariation_ref> vr;
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

    if(delimiter == ";") {
        varset.SetType(CVariation_ref::TData::TSet::eData_set_type_haplotype);
    } else if(delimiter == "+") { 
        varset.SetType(CVariation_ref::TData::TSet::eData_set_type_genotype);
    } else if(delimiter == "(+)") {
        varset.SetType(CVariation_ref::TData::TSet::eData_set_type_individual);
    } else if(delimiter == ",") { 
        //if the context is rna (r.) then this describes multiple products from the same precursor;
        //otherwise this describes mosaic cases
        if(context.GetMolType(false)  == CContext::eMol_r) {
            //Note: GetMolType(check=false) because MolType may not eMol_not_set, as
            //there may not be a sequence in context, e.g.
            //[NM_004004.2:c.35delG,NM_006783.1:c.689_690insT]" - individual
            //elements have their own sequence context, but none at the set level.
            varset.SetType(CVariation_ref::TData::TSet::eData_set_type_products);
        } else {
            varset.SetType(CVariation_ref::TData::TSet::eData_set_type_mosaic);
        }
    } else if(delimiter == "") {
        ;//single-element list
    } else {
        HGVS_THROW(eGrammatic, "Unexpected delimiter " + delimiter);
    }

    vr = x_unwrap_iff_singleton(*vr);
    return vr;
}


CRef<CSeq_feat> CHgvsParser::x_root(TIterator const& i, const CContext& context)
{
    HGVS_ASSERT_RULE(i, eID_root);

    CRef<CVariation_ref> vr = x_list(i, context);

    CVariationUtil::s_FactorOutLocsInPlace(*vr);
    RepackageAssertedSequence(*vr);

    CRef<CSeq_feat> feat(new CSeq_feat);
    feat->SetLocation().Assign(vr->GetLocation());
    vr->ResetLocation();
    feat->SetData().SetVariation(*vr);

    return feat;
}

CRef<CVariation_ref>  CHgvsParser::x_unwrap_iff_singleton(CVariation_ref& v)
{
    if(v.GetData().IsSet() && v.GetData().GetSet().GetVariations().size() == 1) {
        return *v.SetData().SetSet().SetVariations().begin();
    } else {
        return CRef<CVariation_ref>(&v);
    }
}


CRef<CSeq_feat> CHgvsParser::AsVariationFeat(const string& hgvs_expression, TOpFlags flags)
{
    tree_parse_info<> info = pt_parse(hgvs_expression.c_str(), s_grammar, +space_p);
    CRef<CSeq_feat> feat;

    try {
        if(!info.full) {
#if 0
            CNcbiOstrstream ostr;
            tree_to_xml(ostr, info.trees, hgvs_expression.c_str() , CHgvsParser::SGrammar::s_GetRuleNames());
            string tree_str = CNcbiOstrstreamToString(ostr);
#endif
            HGVS_THROW(eGrammatic, "Syntax error at pos " + NStr::NumericToString(info.length + 1));
        } else {
            CContext context(m_scope);
            feat = x_root(info.trees.begin(), context);
            feat->SetData().SetVariation().SetName(hgvs_expression);
        }
    } catch (CException& e) {
        if(flags && fOpFlags_RelaxedAA && NStr::Find(hgvs_expression, "p.")) {
            //expression was protein, try non-hgvs-compliant representation of prots
            string hgvs_expr2 = s_hgvsUCaa2hgvsUL(hgvs_expression);
            TOpFlags flags2 = flags & ~fOpFlags_RelaxedAA; //unset the bit so we don't infinite-recurse
            feat = AsVariationFeat(hgvs_expr2, flags2);
        } else {
            NCBI_RETHROW_SAME(e, "");
        }
    }

    return feat;
}




string CHgvsParser::AsHgvsExpression(const CSeq_feat& variation_feat)
{
    HGVS_THROW(eLogic, "Not implemented");

    string s = x_AsHgvsExpression(variation_feat.GetData().GetVariation(),
                                variation_feat.GetLocation(),
                                true);
    return s;
}

string CHgvsParser::x_AsHgvsExpression(
        const CVariation_ref& variation,
        const CSeq_loc& parent_loc, //if variation has seq-loc set, it will be used instead.
        bool is_top_level)
{
    string outs("");

    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->Assign(variation.IsSetLocation() ? variation.GetLocation() : parent_loc);

    if(variation.GetData().IsSet()) {
        const CVariation_ref::TData::TSet& vset = variation.GetData().GetSet();
        string delim_type =
                vset.GetType() == CVariation_ref::TData::TSet::eData_set_type_compound    ? ""
              : vset.GetType() == CVariation_ref::TData::TSet::eData_set_type_haplotype   ? ";"
              : vset.GetType() == CVariation_ref::TData::TSet::eData_set_type_products
             || vset.GetType() == CVariation_ref::TData::TSet::eData_set_type_mosaic      ? ","
              : vset.GetType() == CVariation_ref::TData::TSet::eData_set_type_alleles     ? "]+["
              : vset.GetType() == CVariation_ref::TData::TSet::eData_set_type_individual  ? "(+)"
              : "](+)[";

        outs = vset.GetType() == CVariation_ref::TData::TSet::eData_set_type_compound ? "" : "[";
        string delim = "";
        ITERATE(CVariation_ref::TData::TSet::TVariations, it, variation.GetData().GetSet().GetVariations()) {
           outs += delim + x_AsHgvsExpression(**it, *loc, false);
           delim = delim_type;
        }
        outs += vset.GetType() == CVariation_ref::TData::TSet::eData_set_type_compound ? "" : "]";

    } else {
        outs = x_InstToString(variation.GetData().GetInstance(), *loc);
                //note: loc is the in/out parameter here
    }

    if(variation.IsSetLocation() || is_top_level) {
        string loc_str = x_SeqLocToStr(*loc, true);
        outs = loc_str + outs;
    }

    return outs;
}



///////////////////////////////////////////////////////////////////////////////
//
// Methods and functions pertaining to converting variation to HGVS
//
///////////////////////////////////////////////////////////////////////////////

string CHgvsParser::x_InstToString(const CVariation_inst& inst, CSeq_loc& loc)
{
    string out = "";

    bool append_delta = false;
    bool flipped_strand = false;

    CRef<CSeq_loc> hgvs_loc(new CSeq_loc);
    hgvs_loc->Assign(loc);
    if(sequence::GetStrand(loc, NULL) == eNa_strand_minus) {
        hgvs_loc->FlipStrand();
        flipped_strand = true;
    }


    if(inst.GetType() == CVariation_inst::eType_identity) {
        out = "=";
    } else if(inst.GetType() == CVariation_inst::eType_inv) {
        out = "inv";
    } else if(inst.GetType() == CVariation_inst::eType_snv) {
        out = x_LocToSeqStr(*hgvs_loc) + ">"; //on the +strand
        append_delta = true;
    } else if(inst.GetType() == CVariation_inst::eType_mnp
              || inst.GetType() == CVariation_inst::eType_delins
              || inst.GetType() == CVariation_inst::eType_transposon)
    {
        append_delta = true;
    } else if(inst.GetType() == CVariation_inst::eType_del) {
        out = "del";
        size_t len = sequence::GetLength(loc, NULL);
        if(len == 1) {
            ; //...del
        } else if(len < 10) {
            //...delACGT
            out += x_LocToSeqStr(*hgvs_loc);
        } else {
            //...del25
            out += NStr::NumericToString(len);
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
            out = "dup";
        } else {
            out = "ins";
            append_delta = true;

            //Whether the second nucleotide will be upstream or downsream of the loc will depend
            //on whether the loc is on plus or minus strand, and where 'this' is in variation-ref.

            if(inst.GetDelta().size() != 2) {
                NCBI_THROW(CException, eUnknown, "Expected inst.delta to be of length 2");
            } else {
                bool ins_after = false;
                if(inst.GetDelta().begin()->GetObject().GetSeq().IsThis()) {
                    ins_after = true;
                } else if(inst.GetDelta().rbegin()->GetObject().GetSeq().IsThis()) {
                    ins_after = false;
                } else {
                    NCBI_THROW(CException, eUnknown, "In insertion, expected expected first or last element of delta to be 'this'");
                }

                if(sequence::GetStrand(loc, NULL) == eNa_strand_minus) {
                    ins_after = !ins_after;
                }

                hgvs_loc->SetInt().SetId().Assign(sequence::GetId(loc, NULL));
                hgvs_loc->SetInt().SetStrand(eNa_strand_plus);
                if(ins_after) {
                    hgvs_loc->SetInt().SetFrom(sequence::GetStop(loc, NULL, eExtreme_Positional));
                    hgvs_loc->SetInt().SetTo(1 + sequence::GetStop(loc, NULL, eExtreme_Positional));
                } else {
                    hgvs_loc->SetInt().SetFrom(-1 + sequence::GetStart(loc, NULL, eExtreme_Positional));
                    hgvs_loc->SetInt().SetTo(sequence::GetStart(loc, NULL, eExtreme_Positional));
                }
            }
        }

    } else if(inst.GetType() == CVariation_inst::eType_microsatellite) {
        out = "";

        // In HGVS land the location is not the whole repeat, as in variation-ref,
        // but the first base of the repeat unit.

        hgvs_loc.Reset(new CSeq_loc);
        hgvs_loc->SetPnt().SetId().Assign(sequence::GetId(loc, NULL));
        hgvs_loc->SetPnt().SetStrand(eNa_strand_plus);
        hgvs_loc->SetPnt().SetPoint(sequence::GetStart(loc, NULL, eExtreme_Biological));

        append_delta = true;
    } else if(inst.GetType() == CVariation_inst::eType_prot_missense
           || inst.GetType() == CVariation_inst::eType_prot_nonsense
           || inst.GetType() == CVariation_inst::eType_prot_neutral)
    {
        append_delta = true;
    } else if(inst.GetType() == CVariation_inst::eType_prot_silent) {
        out = "=";
    } else {
        NCBI_THROW(CException, eUnknown, "Cannot process this type of variation-inst");
    }


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

    loc.Assign(*hgvs_loc);
    return out;
}


string CHgvsParser::x_SeqLiteralToStr(const CSeq_literal& literal, bool flip_strand)
{
    string out("");

    if(literal.IsSetSeq_data()) {
        CRef<CSeq_data> sd(new CSeq_data);

        sd->Assign(literal.GetSeq_data());
        if(flip_strand) {
            CSeqportUtil::ReverseComplement(*sd, sd, 0, literal.GetLength());
        }

        if(  sd->IsIupacna()
          || sd->IsNcbi2na()
          || sd->IsNcbi4na()
          || sd->IsNcbi8na()
          || sd->IsNcbipna())
        {
            CSeqportUtil::Convert(*sd, sd, CSeq_data::e_Iupacna, 0, literal.GetLength() );
            out += sd->GetIupacna().Get();
        } else if(sd->IsIupacaa()
               || sd->IsNcbi8aa()
               || sd->IsNcbieaa()
               || sd->IsNcbipaa()
               || sd->IsNcbistdaa())
        {
            CSeqportUtil::Convert(*sd, sd, CSeq_data::e_Iupacaa, 0, literal.GetLength() );
            out += sd->GetIupacaa().Get();
        }

    } else {
        string multiplier_str = x_IntWithFuzzToStr(
                literal.GetLength(),
                literal.IsSetFuzz() ? &literal.GetFuzz() : NULL);
        out += multiplier_str;
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


string CHgvsParser::x_SeqLocToStr(const CSeq_loc& loc, bool with_header)
{
    const CSeq_id& seq_id = sequence::GetId(loc, NULL);

    string header = x_SeqIdToHgvsHeader(seq_id);

    //for c.-based coordinates, calculate the first pos as start of CDS.
    TSeqPos first_pos = 0;
    if(NStr::EndsWith(header, ":c.")) {
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(seq_id);
        for(CFeat_CI ci(bsh); ci; ++ci) {
            const CMappedFeat& mf = *ci;
            if(mf.GetData().IsCdregion()) {
                first_pos = sequence::GetStart(mf.GetLocation(), NULL, eExtreme_Biological);
            }
        }
    }

    string loc_str = "";
    if(loc.IsPnt()) {
        loc_str = x_SeqPntToStr(loc.GetPnt(), first_pos);
    } else {
        CRef<CSeq_point> p_start(new CSeq_point);
        p_start->SetPoint(sequence::GetStart(loc, NULL, eExtreme_Positional));
        p_start->SetId().Assign(sequence::GetId(loc, NULL));
        if(loc.IsInt() && loc.GetInt().IsSetFuzz_from()) {
            p_start->SetFuzz().Assign(loc.GetInt().GetFuzz_from());
        }
        string s_start = x_SeqPntToStr(*p_start, first_pos);

        CRef<CSeq_point> p_stop(new CSeq_point);
        p_stop->SetPoint(sequence::GetStop(loc, NULL, eExtreme_Positional));
        p_stop->SetId().Assign(sequence::GetId(loc, NULL));
        if(loc.IsInt() && loc.GetInt().IsSetFuzz_to()) {
            p_stop->SetFuzz().Assign(loc.GetInt().GetFuzz_to());
        }
        string s_stop = x_SeqPntToStr(*p_stop, first_pos);

        loc_str = s_start + "_" + s_stop;
    }

    string out = (with_header ? header : "") + loc_str;

    return out;
}


string CHgvsParser::x_SeqPntToStr(const CSeq_point& pnt, TSeqPos first_pos)
{
    CRef<CSeq_point> mapped_pnt;
    long offset(numeric_limits<long>::max());

    mapped_pnt.Reset(new CSeq_point);
    mapped_pnt->Assign(pnt);


    //convert to c. coordinates as necessary.
    //remember that there's no position-zero in HGVS.
    long point_pos = mapped_pnt->GetPoint() + 1; //hgvs absolute coordinates are 1-based.
    point_pos -= first_pos;
    if(point_pos <= 0) {
        point_pos--;
    }

    string outs = NStr::NumericToString(point_pos);
    if(offset != numeric_limits<long>::max()) {
        outs +=  NStr::NumericToString(offset, NStr::fWithSign);
    }

    return outs;
}

string CHgvsParser::x_SeqIdToHgvsHeader(const CSeq_id& id)
{
    string moltype = "";
    CBioseq_Handle bsh = m_scope->GetBioseqHandle(id);
    if(bsh.GetInst_Mol() == CSeq_inst::eMol_aa) {
        moltype = "p.";
    } else if(bsh.GetInst_Mol() == CSeq_inst::eMol_rna) {
        moltype = "c.";

    } else {
        moltype = "g.";
        ITERATE(CSeq_descr::Tdata, it, bsh.GetTopLevelEntry().GetDescr().Get()) {
            const CSeqdesc& desc = **it;
            if(desc.IsSource()
               && desc.GetSource().IsSetGenome()
               && desc.GetSource().GetGenome() == CBioSource::eGenome_mitochondrion)
            {
                moltype = "mt.";
            }
        }
    }

    string accver = sequence::GetAccessionForId(id, *m_scope, sequence::eWithAccessionVersion);
    return accver + ":" + moltype;
}

string CHgvsParser::x_IntWithFuzzToStr(int value, const CInt_fuzz* fuzz)
{
    string out = NStr::IntToString(value);

    if(fuzz) {
        if(fuzz->IsRange()) {
            string from = NStr::IntToString(fuzz->GetRange().GetMin());
            string to = NStr::IntToString(fuzz->GetRange().GetMax());
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
    return out;
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
            d_seq = x_SeqLiteralToStr(d.GetSeq().GetLiteral(), false);
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

};

END_NCBI_SCOPE

