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
#include <objects/seq/seqport_util.hpp>
#include <serial/iterator.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objmgr/seq_vector.hpp>
#include <algo/gnomon/gnomon.hpp>
#include <algo/sequence/orf.hpp>
#include <map>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(gnomon);


bool CTestTranscript::CanTest(const CSerialObject& obj,
                              const CSeqTestContext* ctx) const
{
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if (id  &&  ctx) {
        CBioseq_Handle handle = ctx->GetScope().GetBioseqHandle(*id);
        return handle.CanGetInst_Mol() && handle.GetInst_Mol() == CSeq_inst::eMol_rna;
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
    if (vec.size() > pos + 3 &&
        vec[pos + 3] == 'G') {
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
        = CSeq_loc_CI(feat_iter->GetLocation()).GetEmbeddingSeq_loc();
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
    TSeqPos upstream_length;
    CSeqVector vec = 
        s_GetCdregionPlusUpstream(feat_iter, ctx, upstream_length);
    vec.SetIupacCoding();

    EKozakStrength strength;
    EKozakStrength best_strength = eNone;
    TSeqPos pos_nearest_best_start = 0; // initialize to avoid compiler warning
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
    //creating CHMMParameters object from file is expensive, so
    //created objects are cached in a static map.
    //It is reasonable to expect that the number of parameter files
    //per program is small, and so the cache size does not need
    //to be limited.

    static std::map<string, CRef<CHMMParameters> > s_hmmparams_cache;
    DEFINE_STATIC_FAST_MUTEX(map_mutex); 

    CRef<CHMMParameters> hmm_params;

    if (!ctx->HasKey("gnomon_model_file")) {
        return;
    }


    string model_file_name = (*ctx)["gnomon_model_file"];

    {{
        CFastMutexGuard guard(map_mutex); //released when goes out of scope 4 lines below
        if(s_hmmparams_cache.find(model_file_name) == s_hmmparams_cache.end()) {
            CNcbiIfstream model_file(model_file_name.c_str());
            s_hmmparams_cache[model_file_name] = CRef<CHMMParameters>(new CHMMParameters(model_file));
        }
    }}

    hmm_params = s_hmmparams_cache[model_file_name];
    
    
    const CSeq_loc& cds = feat_iter->GetLocation();

    int gccontent=0;
    double score = CCodingPropensity::GetScore(hmm_params, cds,
                                               ctx->GetScope(), &gccontent);

    // Record results
    result.SetOutput_data()
        .AddField("model_file", model_file_name);
    result.SetOutput_data()
        .AddField("model_percent_gc", gccontent);
    result.SetOutput_data()
        .AddField("score", max(score, -1e100));
    
}


CRef<CSeq_test_result_set>
CTestTranscript_CodingPropensity::RunTest(const CSerialObject& obj,
                                          const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "coding_propensity",
                              s_CodingPropensity);
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


// Determine the position in a cds of the start of a Code-break
inline TSeqPos CodeBreakPosInCds(const CCode_break& code_break,
                                 const CSeq_feat& feat, CScope& scope)
{
    return sequence::LocationOffset(feat.GetLocation(), code_break.GetLoc(),
                                    sequence::eOffset_FromStart, &scope);
}


// Determine whether a Code-break is a selenocysteine
static bool s_IsSelenocysteine(const CCode_break& code_break)
{
    switch (code_break.GetAa().Which()) {
    case CCode_break::TAa::e_Ncbieaa:
        return code_break.GetAa().GetNcbieaa() == 85;
    case CCode_break::TAa::e_Ncbi8aa:
        return code_break.GetAa().GetNcbi8aa() == 24;
    case CCode_break::TAa::e_Ncbistdaa:
        return code_break.GetAa().GetNcbistdaa() == 24;
    case CCode_break::TAa::e_not_set:
    default:
        return false;
    }
}


// Determine whether a position in a CDS feature is the beginning
// of a selenocysteine codon (according to Code-break's)
static bool s_IsSelenocysteine(TSeqPos pos_in_cds, CFeat_CI feat_iter, CScope& scope)
{
    const CSeq_feat& feat = feat_iter->GetOriginalFeature();
    if (!feat.GetData().GetCdregion().IsSetCode_break()) {
        return false;
    }
    ITERATE (CCdregion::TCode_break, code_break,
             feat.GetData().GetCdregion().GetCode_break ()) {
        if (CodeBreakPosInCds(**code_break, feat, scope) == pos_in_cds
            && s_IsSelenocysteine(**code_break)) {
            return true;
        }
    }
    return false;
}


static void s_PrematureStopCodon(const CSeq_id& id, const CSeqTestContext* ctx,
                                 CFeat_CI feat_iter, CSeq_test_result& result)
{
    CConstRef<CGenetic_code> code = 
        s_GetCode(feat_iter->GetData().GetCdregion());
    const CTrans_table& tbl = CGen_code_table::GetTransTable(*code); 
    
    CSeqVector vec(feat_iter->GetLocation(), ctx->GetScope());
    vec.SetIupacCoding();

    TSeqPos start_translating;
    switch (feat_iter->GetData().GetCdregion().GetFrame()) {
    case CCdregion::eFrame_not_set:
    case CCdregion::eFrame_one:
        start_translating = 0;
        break;
    case CCdregion::eFrame_two:
        start_translating = 1;
        break;
    case CCdregion::eFrame_three:
        start_translating = 2;
        break;
    default:
        // should never happen, but handle it to avoid compiler warning
        start_translating = kInvalidSeqPos;
        break;
    }

    bool premature_stop_found = false;
    for (TSeqPos i = start_translating;  i < vec.size() - 3;  i += 3) {
        if (tbl.IsOrfStop(tbl.SetCodonState(vec[i], vec[i + 1],
                                            vec[i + 2]))) {
            if (!premature_stop_found) {
                result.SetOutput_data()
                    .AddField("has_premature_stop_codon", true);
                result.SetOutput_data()
                    .AddField("first_premature_stop_position",
                              static_cast<int>(i));
                premature_stop_found = true;
            }
            // determine whether it's an annotated selenocysteine
            if (!s_IsSelenocysteine(i, feat_iter, ctx->GetScope())) {
                result.SetOutput_data()
                    .AddField("has_premature_stop_codon_not_sec", true);
                result.SetOutput_data()
                    .AddField("first_premature_stop_position_not_sec",
                              static_cast<int>(i));
                return;
            }
        }
    }

    result.SetOutput_data()
        .AddField("has_premature_stop_codon_not_sec", false);
    if (!premature_stop_found) {
        result.SetOutput_data()
            .AddField("has_premature_stop_codon", false);
    }
}


CRef<CSeq_test_result_set>
CTestTranscript_PrematureStopCodon::RunTest(const CSerialObject& obj,
                                            const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "premature_stop_codon",
                              s_PrematureStopCodon);
}


// Walk the replace history to find the latest revision of a sequence
static CConstRef<CSeq_id> s_FindLatest(const CSeq_id& id, CScope& scope)
{
    CConstRef<CSeq_id> latest = sequence::FindLatestSequence(id, scope);
    if ( !latest ) {
        latest.Reset(&id);
    }
    return latest;
}


static void s_CompareProtProdToTrans(const CSeq_id& id,
                                     const CSeqTestContext* ctx,
                                     CFeat_CI feat_iter,
                                     CSeq_test_result& result)
{
    string translation;
    CSeqTranslator::Translate(feat_iter->GetOriginalFeature(), ctx->GetScope(),
                              translation, false /* include_stop */);
    result.SetOutput_data().AddField("length_translation",
                                     int(translation.size()));

    if (!feat_iter->GetOriginalFeature().CanGetProduct()) {
        // can't do comparison if there's no product annotated
        return;
    }

    const CSeq_loc& prod_loc = feat_iter->GetOriginalFeature().GetProduct();
    const CSeq_id& prod_id = sequence::GetId(prod_loc, 0);
    CSeqVector prod_vec(prod_loc, ctx->GetScope());
    prod_vec.SetIupacCoding();

    TSeqPos ident_count = 0;
    for (TSeqPos i = 0;
         i < min(prod_vec.size(), (TSeqPos)translation.size());  ++i) {
        if (prod_vec[i] == translation[i]) {
            ++ident_count;
        }
    }
    
    result.SetOutput_data().AddField("length_annotated_prot_prod",
                                     int(prod_vec.size()));
    result.SetOutput_data()
        .AddField("fraction_identity",
                  double(ident_count)
                  / max(prod_vec.size(), (TSeqPos)translation.size()));

    CConstRef<CSeq_id> updated_id = s_FindLatest(prod_id, ctx->GetScope());
    if (updated_id->Equals(prod_id)) {
        result.SetOutput_data()
            .AddField("fraction_identity_updated_prot_prod",
                      double(ident_count)
                      / max(prod_vec.size(), (TSeqPos)translation.size()));
        result.SetOutput_data().AddField("length_updated_prot_prod",
                                         int(prod_vec.size()));
    } else {
        CBioseq_Handle updated_prod_hand
            = ctx->GetScope().GetBioseqHandle(*updated_id);
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
        .AddField("prot_prod_updated", !updated_id->Equals(prod_id));
    result.SetOutput_data()
        .AddField("updated_prod_id", updated_id->AsFastaString());
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

    //compute trailing a-count
    {{
        int pos(0);
        for(pos = vec.size() - 1;  pos > 0;  --pos) {
            if (vec[pos] != 'A') {
                break;
            }
        }
        result->SetOutput_data().AddField("trailing_a_count",
                                          int(vec.size() - pos - 1));
    }}


    int tail_length(0);
    //compute tail length allowing for mismatches. 
    //Note: there's similar logic for computing genomic polya priming in alignment tests
    {{
        static const int w_match = 1;
        static const int w_mismatch = -4;
        static const int x_dropoff = 15;

        int best_pos = NPOS;
        int best_score = 0;
        int curr_score = 0;

        for(int curr_pos = vec.size() - 1; 
            curr_pos > 0 && curr_score + x_dropoff > best_score; 
            --curr_pos) 
        {
            curr_score += vec[curr_pos] == 'A' ? w_match : w_mismatch;
            if(curr_score >= best_score) {
                best_score = curr_score;
                best_pos = curr_pos;
            }
        }
        tail_length = (best_pos == NPOS) ? 0 : vec.size() - best_pos; 
        result->SetOutput_data().AddField("tail_length", tail_length);
    }}

    
    //find signal
    {{
        static string patterns[] = { 
            "AATAAA",
            "ATTAAA",   
            "AGTAAA",
            "TATAAA",   
            "CATAAA",   
            "GATAAA",   
            "AATATA",   
            "AATACA",   
            "AATAGA",   
            "ACTAAA",   
            "AAGAAA",   
            "AATGAA"
        };  

        size_t window = 50; //serch within 50 bases upstream of polya-site
        size_t end_pos = vec.size() - 1 - tail_length;
        size_t begin_pos = end_pos > window ? end_pos - window : 0;
 
        string seq;
        vec.GetSeqData(begin_pos, end_pos, seq);

        for(int ii = 0; ii < 12; ii++) {    
            size_t pos = NStr::Find(seq, patterns[ii], 0, NPOS, NStr::eLast);
            if(pos != NPOS) {
                result->SetOutput_data().AddField("signal_pos", static_cast<int>(pos + begin_pos));
                result->SetOutput_data().AddField("is_canonical_pas", (ii <= 1)); //AATAAA or ATTAAA
                break;
            }   
        }   
    }}

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

    // Look for ORFs starting with any sense codon
    vector<CRef<CSeq_loc> > orfs;
    COrf::FindOrfs(vec, orfs);
    TSeqPos max_orf_length_forward = 0;
    TSeqPos max_orf_length_either = 0;
    TSeqPos largest_forward_orf_end = 0;  // intialized to avoid comp. warning
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

    // Look for ORFs starting with ATG
    orfs.clear();
    vector<string> allowable_starts;
    allowable_starts.push_back("ATG");
    COrf::FindOrfs(vec, orfs, 3, 1, allowable_starts);
    max_orf_length_forward = 0;
    max_orf_length_either = 0;
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

    result->SetOutput_data().AddField("max_atg_orf_length_forward_strand",
                                      int(max_orf_length_forward));
    result->SetOutput_data().AddField("largest_forward_atg_orf_end_pos",
                                      int(largest_forward_orf_end));
    result->SetOutput_data().AddField("max_atg_orf_length_either_strand",
                                      int(max_orf_length_either));

    return ref;
}


static void s_Code_break(const CSeq_id& id, const CSeqTestContext* ctx,
                         CFeat_CI feat_iter, CSeq_test_result& result)
{
    int count, not_start_not_sec_count;
    if (feat_iter->GetData().GetCdregion().IsSetCode_break()) {
        count = feat_iter->GetData().GetCdregion().GetCode_break().size();
        not_start_not_sec_count = 0;
        ITERATE (CCdregion::TCode_break, code_break,
                 feat_iter->GetData().GetCdregion().GetCode_break()) {
            TSeqPos pos = CodeBreakPosInCds(**code_break,
                                            feat_iter->GetOriginalFeature(),
                                            ctx->GetScope());
            if (pos != 0 && !s_IsSelenocysteine(**code_break)) {
                ++not_start_not_sec_count;
            }
        }
    } else {
        count = 0;
        not_start_not_sec_count = 0;
    }

    result.SetOutput_data()
        .AddField("code_break_count", count);
    result.SetOutput_data()
        .AddField("code_break_not_start_not_sec_count",
                  not_start_not_sec_count);
}


CRef<CSeq_test_result_set>
CTestTranscript_Code_break::RunTest(const CSerialObject& obj,
                                    const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "code_break",
                              s_Code_break);
}


static void s_OrfExtension(const CSeq_id& id,
                           const CSeqTestContext* ctx,
                           CFeat_CI feat_iter,
                           CSeq_test_result& result)
{
    TSeqPos upstream_length;
    CSeqVector vec = 
        s_GetCdregionPlusUpstream(feat_iter, ctx, upstream_length);
    vec.SetIupacCoding();

    EKozakStrength strength;
    vector<int> starts(eStrong + 1, upstream_length);
    string codon;
    for (int i = upstream_length - 3;  i >= 0;  i -= 3) {
        vec.GetSeqData(i, i + 3, codon);
        if (codon == "ATG") {
            strength = s_GetKozakStrength(vec, i);
            starts[strength] = i;
        }
        if (codon == "TAA" || codon == "TAG" || codon == "TGA") {
            break;
        }
    }

    // MSS-59 
    // Count the total number of 'ATG' triplets found in the 5'UTR of a 
    // transcript, in frames 1, 2, and 3.
    int upstream_utr_atg_count(0);
    for (int i = upstream_length - 3;  i >= 0;  i -= 1) {
        vec.GetSeqData(i, i + 3, codon);
        if (codon == "ATG") {
            upstream_utr_atg_count++;
        }
    }

    result.SetOutput_data()
        .AddField("max_extension_weak_kozak",
                  static_cast<int>(upstream_length - starts[eWeak]));
    result.SetOutput_data()
        .AddField("max_extension_moderate_kozak",
                  static_cast<int>(upstream_length - starts[eModerate]));
    result.SetOutput_data()
        .AddField("max_extension_strong_kozak",
                  static_cast<int>(upstream_length - starts[eStrong]));
    result.SetOutput_data()
        .AddField("upstream_utr_atg_count",
                  upstream_utr_atg_count);
}


CRef<CSeq_test_result_set>
CTestTranscript_OrfExtension::RunTest(const CSerialObject& obj,
                                              const CSeqTestContext* ctx)
{
    return x_TestAllCdregions(obj, ctx, "orf_extension",
                              s_OrfExtension);
}


static TSeqPos s_CountAmbiguities(const CSeqVector& vec)
{
    CSeqVector vec_copy(vec);
    vec_copy.SetIupacCoding();
    string seq;
    vec_copy.GetSeqData(0, vec_copy.size(), seq);

    CSeq_data in_seq, out_seq;
    in_seq.SetIupacna().Set(seq);
    vector<TSeqPos> out_indices;

    return CSeqportUtil::GetAmbigs(in_seq, &out_seq, &out_indices);
}


static void s_CdsCountAmbiguities(const CSeq_id& id,
                                  const CSeqTestContext* ctx,
                                  CFeat_CI feat_iter, CSeq_test_result& result)
{
    CSeqVector vec(feat_iter->GetLocation(), ctx->GetScope());
    result.SetOutput_data()
        .AddField("cds_ambiguity_count",
                  static_cast<int>(s_CountAmbiguities(vec)));
}


CRef<CSeq_test_result_set>
CTestTranscript_CountAmbiguities::RunTest(const CSerialObject& obj,
                                          const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> rv;
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if ( !id  ||  !ctx ) {
        return rv;
    }

    // count for each coding region
    rv = x_TestAllCdregions(obj, ctx, "count_ambiguities",
                            s_CdsCountAmbiguities);

    // count for entire transcript
    if (!rv) {
        rv.Reset(new CSeq_test_result_set());
    }
    CBioseq_Handle hand = ctx->GetScope().GetBioseqHandle(*id);
    CSeqVector vec = hand.GetSeqVector();
    CRef<CSeq_test_result> result = x_SkeletalTestResult("count_ambiguities");
    rv->Set().push_back(result);
    result->SetOutput_data()
        .AddField("ambiguity_count",
                  static_cast<int>(s_CountAmbiguities(vec)));
    return rv;
}


END_NCBI_SCOPE
