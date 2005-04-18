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
 * Authors:  Mike DiCuccio, Josh Cherry
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/seqqa/xcript_tests.hpp>
#include <corelib/ncbitime.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqtest/Seq_test_result.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <serial/iterator.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objmgr/seq_vector.hpp>
#include "../gnomon/gene_finder.hpp"
#include <algo/sequence/orf.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


bool CTestTranscript::CanTest(const CSerialObject& obj,
                              const CSeqTestContext* ctx) const
{
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if (id  &&  ctx) {
        CBioseq_Handle handle = ctx->GetScope().GetBioseqHandle(*id);

        CSeqdesc_CI iter(handle, CSeqdesc::e_Molinfo);
        for ( ;  iter;  ++iter) {
            const CMolInfo& info = iter->GetMolinfo();
            if (info.GetBiomol() == CMolInfo::eBiomol_mRNA) {
                return true;
            }
        }
    }
    return false;
}


CRef<CSeq_test_result_set>
CTestTranscript_CountCdregions::RunTest(const CSerialObject& obj,
                                       const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> ref;
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if ( !id  ||  !ctx ) {
        return ref;
    }

    ref.Reset(new CSeq_test_result_set());

    CRef<CSeq_test_result> result = x_SkeletalTestResult("count_cdregions");
    ref->Set().push_back(result);

    SAnnotSelector sel;
    sel.SetFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    sel.SetResolveDepth(0);
    CSeq_loc loc;
    loc.SetWhole().Assign(*id);
    CFeat_CI feat_iter(ctx->GetScope(), loc, sel);

    result->SetOutput_data()
        .AddField("count", (int) feat_iter.GetSize());
    return ref;
}


// A simplistic Kozak strength.  Best is RNNXXXG, where
// XXX is the start codon.  This is "strong".  Having just
// R or just G is "moderate", and having neither is "weak".
enum EKozakStrength
{
    eNone,     // used to indicate that no start has been seen
    eWeak,
    eModerate,
    eStrong
};


// vec must be set to IUPAC coding
EKozakStrength s_GetKozakStrength(const CSeqVector& vec, TSeqPos pos)
{
    int score = eWeak;
    if (pos >= 3 &&
        (vec[pos - 3] == 'A' || vec[pos - 3] == 'G')) {
        ++score;
    }
    if (vec[pos + 3] == 'G') {
        ++score;
    }
    return EKozakStrength(score);
}


string s_KozakStrengthToString(EKozakStrength strength)
{
    switch (strength) {
    default:
    case eNone:
        return "none";

    case eWeak:
        return "weak";

    case eModerate:
        return "moderate";

    case eStrong:
        return "strong";
    }
}


// Return a SeqVector describing the coding regioin, in the
// correct orientation, plus the upstream region.
// Put the length of the upstream region in upstream_length
static CSeqVector s_GetCdregionPlusUpstream(CFeat_CI feat_iter,
                                            const CSeqTestContext* ctx,
                                            TSeqPos& upstream_length)
{
    CScope& scope = ctx->GetScope();
    const CSeq_loc& first_cds_loc
        = CSeq_loc_CI(feat_iter->GetLocation()).GetSeq_loc();
    CRef<CSeq_loc> upstr(new CSeq_loc);
    const CSeq_id& id = sequence::GetId(first_cds_loc, 0);
    upstr->SetInt().SetId().Assign(id);
    if (sequence::GetStrand(first_cds_loc) == eNa_strand_minus) {
        upstr->SetInt().SetStrand(eNa_strand_minus);
        upstr->SetInt().SetFrom(sequence::GetStop(first_cds_loc, 0) + 1);
        upstr->SetInt().SetTo(sequence::GetLength(id, &scope) - 1);
    } else {
        upstr->SetInt().SetFrom(0);
        upstr->SetInt().SetTo(sequence::GetStart(first_cds_loc, 0) - 1);
    }
    CSeq_loc loc;
    loc.SetMix().AddSeqLoc(*upstr);
    loc.SetMix().AddSeqLoc(feat_iter->GetLocation());
    CSeqVector vec(loc, scope);
    upstream_length = sequence::GetLength(*upstr, 0);
    return vec;
}


// If set, return genetic code.  Otherwise return
// "standard" genetic code.
CConstRef<CGenetic_code> s_GetCode(const CCdregion& cdr)
{
    if (cdr.CanGetCode()) {
        return CConstRef<CGenetic_code>(&cdr.GetCode());
    }
    CRef<CGenetic_code> standard(new CGenetic_code);
    CRef<CGenetic_code::C_E> code_id(new CGenetic_code::C_E);
    code_id->SetId(1);
    standard->Set().push_back(code_id);
    return standard;
}


static void s_CdsFlags(const CSeq_id& id, const CSeqTestContext* ctx,
                       CFeat_CI feat_iter, CSeq_test_result& result)
{
    result.SetOutput_data()
        .AddField("is_partial",
                  feat_iter->GetPartial());
    result.SetOutput_data()
        .AddField("is_pseudo",
                  feat_iter->IsSetPseudo() && feat_iter->GetPseudo());
    result.SetOutput_data()
        .AddField("is_except",
                  feat_iter->IsSetExcept() && feat_iter->GetExcept());
}


CRef<CSeq_test_result_set>
CTestTranscript_CdsFlags::RunTest(const CSerialObject& obj,
                                  const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "cds_flags", s_CdsFlags);
}


static void s_InframeUpstreamStart(const CSeq_id& id,
                                   const CSeqTestContext* ctx,
                                   CFeat_CI feat_iter,
                                   CSeq_test_result& result)
{
    CConstRef<CGenetic_code> code = 
        s_GetCode(feat_iter->GetData().GetCdregion());
    const CTrans_table& tbl = CGen_code_table::GetTransTable(*code); 
    
    TSeqPos upstream_length;
    CSeqVector vec = 
        s_GetCdregionPlusUpstream(feat_iter, ctx, upstream_length);
    vec.SetIupacCoding();

    EKozakStrength strength;
    EKozakStrength best_strength = eNone;
    TSeqPos pos_nearest_best_start;
    for (int i = upstream_length - 3;  i >= 0;  i -= 3) {
        if (vec[i] == 'A' && vec[i + 1] == 'T' && vec[i + 2] == 'G') {
            strength = s_GetKozakStrength(vec, i);
            if (strength > best_strength) {
                best_strength = strength;
                pos_nearest_best_start = i;
            }
        }
    }
    result.SetOutput_data()
        .AddField("inframe_upstream_start_exists", best_strength != eNone);
    if (best_strength != eNone) {
        result.SetOutput_data()
            .AddField("inframe_upstream_start_best_kozak_strength",
                      s_KozakStrengthToString(best_strength));
        result.SetOutput_data()
            .AddField("nearest_best_upstream_start_distance",
                      int(upstream_length - pos_nearest_best_start - 3));
    }
}


CRef<CSeq_test_result_set>
CTestTranscript_InframeUpstreamStart::RunTest(const CSerialObject& obj,
                                              const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "inframe_upstream_start",
                              s_InframeUpstreamStart);
}


static void s_InframeUpstreamStop(const CSeq_id& id,
                                  const CSeqTestContext* ctx,
                                  CFeat_CI feat_iter, CSeq_test_result& result)
{
    CConstRef<CGenetic_code> code = 
        s_GetCode(feat_iter->GetData().GetCdregion());
    const CTrans_table& tbl = CGen_code_table::GetTransTable(*code); 

    TSeqPos upstream_length;
    CSeqVector vec = 
        s_GetCdregionPlusUpstream(feat_iter, ctx, upstream_length);
    vec.SetIupacCoding();

    for (int i = upstream_length - 3;  i >= 0;  i -= 3) {
        if (tbl.IsOrfStop(tbl.SetCodonState(vec[i], vec[i + 1],
                                            vec[i + 2]))) {
            result.SetOutput_data()
                .AddField("inframe_upstream_stop_exists",
                          true);
            result.SetOutput_data()
                .AddField("nearest_inframe_upstream_stop_distance",
                          int(upstream_length - i - 3));
            return;
        }
    }
    result.SetOutput_data()
        .AddField("inframe_upstream_stop_exists",
                  false);
}


CRef<CSeq_test_result_set>
CTestTranscript_InframeUpstreamStop::RunTest(const CSerialObject& obj,
                                             const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "inframe_upstream_stop",
                              s_InframeUpstreamStop);
}


static void s_CodingPropensity(const CSeq_id& id, const CSeqTestContext* ctx,
                               CFeat_CI feat_iter, CSeq_test_result& result)
{
    // Need to know GC content in order to load correct models.
    // Compute on the whole transcript, not just CDS.
    CBioseq_Handle xcript_hand = ctx->GetScope().GetBioseqHandle(id);
    CSeqVector xcript_vec = xcript_hand.GetSeqVector();
    xcript_vec.SetIupacCoding();
    unsigned int gc_count = 0;
    CSeqVector_CI xcript_iter(xcript_vec);
    for( ;  xcript_iter;  ++xcript_iter) {
        if (*xcript_iter == 'G' || *xcript_iter == 'C') {
            ++gc_count;
        }
    }
    unsigned int gc_percent(static_cast<unsigned int>
                            ((100.0 * gc_count) / xcript_vec.size() + 0.5));
    
    // Load models from file
    if (!ctx->HasKey("gnomon_model_file")) {
        return;
    }
    string model_file_name = (*ctx)["gnomon_model_file"];
    MC3_CodingRegion<5> coding_mc(model_file_name, gc_percent);
    MC_NonCodingRegion<5> non_coding_mc(model_file_name, gc_percent);

    // Represent coding sequence as enum Nucleotides
    CSeqVector vec(feat_iter->GetLocation(), ctx->GetScope());
    vec.SetIupacCoding();
    vector<char> seq;
    seq.reserve(vec.size());
    CSeqVector_CI iter(vec);
    for( ;  iter;  ++iter) {
        switch (*iter) {
        case 'A':
            seq.push_back(nA);
            break;
        case 'C':
            seq.push_back(nC);
            break;
        case 'G':
            seq.push_back(nG);
            break;
        case 'T':
            seq.push_back(nT);
            break;
        default:
            seq.push_back(nN);
            break;
        }
    }

    // Sum coding and non-coding scores across coding sequence.
    // Don't include stop codon!
    double coding_score = 0;
    for (unsigned int i = 5;  i < seq.size() - 3;  ++i) {
        coding_score += coding_mc.Score(seq, i, i % 3);
    }
    
    double non_coding_score = 0;
    for (unsigned int i = 5;  i < seq.size() - 3;  ++i) {
        non_coding_score += non_coding_mc.Score(seq, i);
    }
    
    // Record results
    result.SetOutput_data()
        .AddField("model_file", model_file_name);
    result.SetOutput_data()
        .AddField("model_percent_gc", (int) gc_percent);
    result.SetOutput_data()
        .AddField("score", max(coding_score - non_coding_score, -1e100));
    
}


CRef<CSeq_test_result_set>
CTestTranscript_CodingPropensity::RunTest(const CSerialObject& obj,
                                          const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "coding_propensity", s_CodingPropensity);
}


CRef<CSeq_test_result_set>
CTestTranscript_TranscriptLength::RunTest(const CSerialObject& obj,
                                          const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> ref;
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if ( !id  ||  !ctx ) {
        return ref;
    }

    ref.Reset(new CSeq_test_result_set());

    CRef<CSeq_test_result> result = x_SkeletalTestResult("transcript_length");
    ref->Set().push_back(result);

    int len = ctx->GetScope()
        .GetBioseqHandle(dynamic_cast<const CSeq_id&>(obj)).GetInst_Length();
    result->SetOutput_data()
        .AddField("length", len);
    return ref;
}


static void s_CdsLength(const CSeq_id& id, const CSeqTestContext* ctx,
                        CFeat_CI feat_iter, CSeq_test_result& result)
{
    result.SetOutput_data()
        .AddField("length",
                  (int)sequence::GetLength(feat_iter->GetLocation(), 0));
}


CRef<CSeq_test_result_set>
CTestTranscript_TranscriptCdsLength::RunTest(const CSerialObject& obj,
                                             const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "cds_length", s_CdsLength);
}


static void s_Utrs(const CSeq_id& id, const CSeqTestContext* ctx,
                   CFeat_CI feat_iter, CSeq_test_result& result)
{
    const CSeq_loc& loc = feat_iter->GetLocation();
    TSeqPos cds_from = sequence::GetStart(loc, 0);
    TSeqPos cds_to   = sequence::GetStop(loc, 0);
    int xcript_len = ctx->GetScope().GetBioseqHandle(id).GetInst_Length();
    result.SetOutput_data().AddField("length_5_prime_utr", (int) cds_from);
    result.SetOutput_data().AddField("length_3_prime_utr",
                                     (int) (xcript_len - cds_to - 1));
}


CRef<CSeq_test_result_set>
CTestTranscript_Utrs::RunTest(const CSerialObject& obj,
                              const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "utrs", s_Utrs);
}


static void s_CdsStartCodon(const CSeq_id& id, const CSeqTestContext* ctx,
                            CFeat_CI feat_iter, CSeq_test_result& result)
{
    CConstRef<CGenetic_code> code = 
        s_GetCode(feat_iter->GetData().GetCdregion());
    const CTrans_table& tbl = CGen_code_table::GetTransTable(*code); 

    TSeqPos upstream_length;
    CSeqVector vec = 
        s_GetCdregionPlusUpstream(feat_iter, ctx, upstream_length);
    vec.SetIupacCoding();

    string seq;
    vec.GetSeqData(upstream_length, upstream_length + 3, seq);
    // is this an officially sanctioned start?
    result.SetOutput_data()
        .AddField("is_start",
                  tbl.IsOrfStart(tbl.SetCodonState(seq[0], seq[1], seq[2])));
    // record it, whatever it is
    result.SetOutput_data()
        .AddField("first_codon", seq);

    result.SetOutput_data()
        .AddField("kozak_strength",
                  s_KozakStrengthToString(s_GetKozakStrength(vec, upstream_length)));
}


CRef<CSeq_test_result_set>
CTestTranscript_CdsStartCodon::RunTest(const CSerialObject& obj,
                                       const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "cds_start_codon", s_CdsStartCodon);
}


static void s_CdsStopCodon(const CSeq_id& id, const CSeqTestContext* ctx,
                           CFeat_CI feat_iter, CSeq_test_result& result)
{
    CConstRef<CGenetic_code> code = 
        s_GetCode(feat_iter->GetData().GetCdregion());
    const CTrans_table& tbl = CGen_code_table::GetTransTable(*code); 
    
    CSeqVector vec(feat_iter->GetLocation(), ctx->GetScope());
    vec.SetIupacCoding();
    string seq;
    vec.GetSeqData(vec.size() - 3, vec.size(), seq);
    result.SetOutput_data()
        .AddField("is_stop",
                  tbl.IsOrfStop(tbl.SetCodonState(seq[0], seq[1], seq[2])));
}


CRef<CSeq_test_result_set>
CTestTranscript_CdsStopCodon::RunTest(const CSerialObject& obj,
                                      const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "cds_stop_codon", s_CdsStopCodon);
}


static void s_PrematureStopCodon(const CSeq_id& id, const CSeqTestContext* ctx,
                                 CFeat_CI feat_iter, CSeq_test_result& result)
{
    CConstRef<CGenetic_code> code = 
        s_GetCode(feat_iter->GetData().GetCdregion());
    const CTrans_table& tbl = CGen_code_table::GetTransTable(*code); 
    
    CSeqVector vec(feat_iter->GetLocation(), ctx->GetScope());
    vec.SetIupacCoding();

    for (int i = 0;  i < vec.size() - 3;  i += 3) {
        if (tbl.IsOrfStop(tbl.SetCodonState(vec[i], vec[i + 1],
                                            vec[i + 2]))) {
            result.SetOutput_data()
                .AddField("has_premature_stop_codon", true);
            result.SetOutput_data()
                .AddField("first_premature_stop_position", i);
            return;
        }
    }

    result.SetOutput_data()
        .AddField("has_premature_stop_codon", false);
}


CRef<CSeq_test_result_set>
CTestTranscript_PrematureStopCodon::RunTest(const CSerialObject& obj,
                                            const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "premature_stop_codon",
                              s_PrematureStopCodon);
}


// Walk the replace history to find the latest revision of a sequence
static const CSeq_id& s_FindLatest(const CSeq_id& id, CScope& scope)
{
    CBioseq_Handle hand = scope.GetBioseqHandle(id);
    if (hand.IsSetInst() && hand.GetInst().IsSetHist()
        && hand.GetInst().GetHist().IsSetReplaced_by()) {
        // This assumes there's just one id in Replaced-by;
        // what to do if there are more?
        return s_FindLatest(*hand.GetInst().GetHist().GetReplaced_by()
                            .GetIds().front(), scope);
    } else {
        return id;
    }
}


static void s_CompareProtProdToTrans(const CSeq_id& id,
                                     const CSeqTestContext* ctx,
                                     CFeat_CI feat_iter,
                                     CSeq_test_result& result)
{
    const CSeq_loc& prod_loc = feat_iter->GetOriginalFeature().GetProduct();
    const CSeq_id& prod_id = sequence::GetId(prod_loc, 0);
    CSeqVector prod_vec(prod_loc, ctx->GetScope());
    prod_vec.SetIupacCoding();

    string translation;
    CCdregion_translate::TranslateCdregion(translation,
                                           feat_iter->GetOriginalFeature(),
                                           ctx->GetScope(), false);

    TSeqPos ident_count = 0;
    for (TSeqPos i = 0;
         i < min(prod_vec.size(), (TSeqPos)translation.size());  ++i) {
        if (prod_vec[i] == translation[i]) {
            ++ident_count;
        }
    }
    
    result.SetOutput_data().AddField("length_annotated_prot_prod",
                                     int(prod_vec.size()));
    result.SetOutput_data().AddField("length_translation",
                                     int(translation.size()));

    result.SetOutput_data()
        .AddField("fraction_identity",
                  double(ident_count)
                  / max(prod_vec.size(), (TSeqPos)translation.size()));

    const CSeq_id& updated_id = s_FindLatest(prod_id, ctx->GetScope());
    if (updated_id.Equals(prod_id)) {
        result.SetOutput_data()
            .AddField("fraction_identity_updated_prot_prod",
                      double(ident_count)
                      / max(prod_vec.size(), (TSeqPos)translation.size()));
        result.SetOutput_data().AddField("length_updated_prot_prod",
                                         int(prod_vec.size()));
    } else {
        CBioseq_Handle updated_prod_hand
            = ctx->GetScope().GetBioseqHandle(updated_id);
        CSeqVector updated_prod_vec = updated_prod_hand.GetSeqVector();
        updated_prod_vec.SetIupacCoding();
        TSeqPos ident_count = 0;
        for (TSeqPos i = 0;
             i < min(updated_prod_vec.size(), (TSeqPos)translation.size());
             ++i) {
            if (updated_prod_vec[i] == translation[i]) {
                ++ident_count;
            }
        }
        result.SetOutput_data()
            .AddField("fraction_identity_updated_prot_prod",
                      double(ident_count)
                      / max(updated_prod_vec.size(),
                            (TSeqPos)translation.size()));
        result.SetOutput_data().AddField("length_updated_prot_prod",
                                         int(updated_prod_vec.size()));
    }
    result.SetOutput_data()
        .AddField("prot_prod_updated", !updated_id.Equals(prod_id));
    result.SetOutput_data()
        .AddField("updated_prod_id", updated_id.AsFastaString());
}


CRef<CSeq_test_result_set>
CTestTranscript_CompareProtProdToTrans::RunTest(const CSerialObject& obj,
                                                const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "compare_prot_prod_to_trans",
                              s_CompareProtProdToTrans);
}


CRef<CSeq_test_result_set>
CTestTranscript_PolyA::RunTest(const CSerialObject& obj,
                                          const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> ref;
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if ( !id  ||  !ctx ) {
        return ref;
    }

    ref.Reset(new CSeq_test_result_set());

    CRef<CSeq_test_result> result = x_SkeletalTestResult("poly_a");
    ref->Set().push_back(result);

    CBioseq_Handle xcript_hand = ctx->GetScope().GetBioseqHandle(*id);
    CSeqVector vec = xcript_hand.GetSeqVector();
    vec.SetIupacCoding();

    int pos;
    for (pos = vec.size() - 1;  pos > 0;  --pos) {
        if (vec[pos] != 'A') {
            break;
        }
    }
    result->SetOutput_data().AddField("trailing_a_count",
                                      int(vec.size() - pos - 1));
    return ref;
}


CRef<CSeq_test_result_set>
CTestTranscript_Orfs::RunTest(const CSerialObject& obj,
                              const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> ref;
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if ( !id  ||  !ctx ) {
        return ref;
    }

    ref.Reset(new CSeq_test_result_set());

    CRef<CSeq_test_result> result = x_SkeletalTestResult("orfs");
    ref->Set().push_back(result);

    CBioseq_Handle xcript_hand = ctx->GetScope().GetBioseqHandle(*id);
    CSeqVector vec = xcript_hand.GetSeqVector();
    vec.SetIupacCoding();

    vector<CRef<CSeq_loc> > orfs;
    COrf::FindOrfs(vec, orfs);
    TSeqPos max_orf_length_forward = 0;
    TSeqPos max_orf_length_either = 0;
    TSeqPos largest_forward_orf_end;
    ITERATE (vector<CRef<CSeq_loc> >, orf, orfs) {
        TSeqPos orf_length = sequence::GetLength(**orf, 0);
        max_orf_length_either = max(max_orf_length_either, orf_length);
        if ((*orf)->GetInt().GetStrand() != eNa_strand_minus) {
            if (orf_length > max_orf_length_forward) {
                max_orf_length_forward = orf_length;
                largest_forward_orf_end = (*orf)->GetInt().GetTo();
            }
            max_orf_length_forward = max(max_orf_length_forward, orf_length);
        }
    }

    result->SetOutput_data().AddField("max_orf_length_forward_strand",
                                      int(max_orf_length_forward));
    result->SetOutput_data().AddField("largest_forward_orf_end_pos",
                                      int(largest_forward_orf_end));
    result->SetOutput_data().AddField("max_orf_length_either_strand",
                                      int(max_orf_length_either));
    return ref;
}


static void s_Code_break(const CSeq_id& id, const CSeqTestContext* ctx,
                         CFeat_CI feat_iter, CSeq_test_result& result)
{
    int count;
    if (feat_iter->GetData().GetCdregion().IsSetCode_break()) {
        count = feat_iter->GetData().GetCdregion().GetCode_break().size();
    } else {
        count = 0;
    }

    result.SetOutput_data()
        .AddField("code_break_count", count);
}


CRef<CSeq_test_result_set>
CTestTranscript_Code_break::RunTest(const CSerialObject& obj,
                                    const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "code_break",
                              s_Code_break);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2005/04/18 18:33:15  jcherry
 * Explicit cast to eliminate compiler warning
 *
 * Revision 1.12  2004/11/18 21:27:40  grichenk
 * Removed default value for scope argument in seq-loc related functions.
 *
 * Revision 1.11  2004/11/08 17:51:01  dicuccio
 * s_KozakStrengthToString(): all return paths covered...
 *
 * Revision 1.10  2004/11/03 21:03:08  jcherry
 * Record id of updated protein
 *
 * Revision 1.9  2004/10/25 16:47:17  jcherry
 * Added comparison of translation to updated protein product
 *
 * Revision 1.8  2004/10/21 21:02:03  jcherry
 * Added test for code-breaks in CDS feature and recording of
 * lengths of protein product and translation.
 *
 * Revision 1.7  2004/10/18 21:07:46  jcherry
 * Added premature stop codon test
 *
 * Revision 1.6  2004/10/13 15:49:06  jcherry
 * Use resolve depth of zero rather than adaptive depth
 *
 * Revision 1.5  2004/10/12 21:50:25  jcherry
 * s_GetCdregionPlusUpstream: don't assume that components of
 * CDS location are intervals
 *
 * Revision 1.4  2004/10/10 21:49:31  jcherry
 * Don't call CMappedFeat::GetMappedFeature; as of recent changes, this
 * apparently doesn't work when the feature has not been mapped.
 *
 * Revision 1.3  2004/10/07 13:38:56  ucko
 * s_CompareProtProdToTrans: make sure to use TSeqPos uniformly.
 *
 * Revision 1.2  2004/10/06 21:49:05  jcherry
 * Use "adaptive depth" (apparently will be necessary for use
 * of "fake" transcripts representing genomic annotation)
 *
 * Revision 1.1  2004/10/06 19:57:15  jcherry
 * Initial version
 *
 * ===========================================================================
 */
