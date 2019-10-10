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
 *  Author : Alex Astashyn
 */

#include <ncbi_pch.hpp>
#include <algo/align/util/best_placement.hpp>
//#include "best_placement.hpp"

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Splice_site.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <cmath>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

/*
    For high-identity alignments mismatches carry more signal, i.e.
    short high-identity partial is better than a long full-coverage
    placement that is likely a paralog.

    However, very short 100% identity is worse than high-identity
    full-coverage alignment.

    For low-identity alignments (e.g. cross-species) mismatches matter less, 
    and other attributes, e.g. coverage, matter more.

    For high-identity alignments indels matter more than mismatches,
    but in gappy PacBio alignments indels don't carry a lot of signal.

    Spread-out defects matter more than clustered, since the latter is
    more likely due to low-qual sequencing or alignment artifact 
    rather than true SNPs.

    -----------------------------------------------------------------------

    Odds are multiplicative; log-odds are additive.

    gaps and mismatches are in separate categories such that
    if an alignment is a gappy crud from pacbio gaps don't
    overshadow everything else.

    score = log(matches+polya)   - log(mismatches)
          + log(ug_aln_len)      - log(gaps + 4*gapopens + 8*frameshifting_gapopens)
          + log(ug_aln_len)      - log(unaligned_len)
          + log(cons_spl)        - log(ncons_spl) 
          + log(good_exons)      - log(partial_or_short_or_downstreamof_long_intron)
          + log(L)               - log(1) // L is longest matchrun capped at ungapped_len/3
*/

///////////////////////////////////////////////////////////////////////////

struct SAlignmentScoringModel
{
    int operator() (const CSeq_align& aln) const
    {
        return (int)round(100 * GetScore(aln));
    }
    
    ///////////////////////////////////////////////////////////////////////
   
private:

    // pair of sum of positives and sum of negatives for
    // some score-category (e.g matches vs. mismatches)
    struct odds
    {
        double pos, neg; // sum of absolute-values

        odds(double pos_ = 0.0, double neg_ = 0.0) : pos(pos_), neg(neg_) {}
       
        void operator += (double x)     { (x > 0 ? pos : neg) += fabs(x);  }
        void operator += (const odds o) { pos += o.pos;  neg  += o.neg;    }
        
        double logodds(int a = 2) const
        {
            if(pos < 0 || neg < 0) {
                NCBI_USER_THROW("Invalid state of odds: " + this->AsString());
            }
            return log( (a+pos)/(a+neg) );
        }

        string AsString() const
        {
            return "(+" + std::to_string(pos) 
                + ", -" + std::to_string(neg) + ")";
        }
    };

    static double as_logodds(double ratio)
    {
        _ASSERT(0 < ratio && ratio < 1);
        return log( ratio / (1 - ratio) );
    }

    ///////////////////////////////////////////////////////////////////////

    static double clamp(double x, double lo, double hi) 
    {
        return x < lo ? lo
             : x > hi ? hi
             :          x;
    }
 

    double GetScore(const CSeq_align& aln) const
    {
        const bool is_prot = isProtSS(aln);

        const auto      gapped_len = aln.GetAlignLength(true);
        const auto    ungapped_len = aln.GetAlignLength(false);
        const auto        num_gaps = gapped_len - ungapped_len;
        const auto    num_gapopens = aln.GetNumGapOpenings();
        const auto num_frameshifts = !is_prot ? 0 : aln.GetNumFrameshifts();
        const auto       polya_len = GetPolyA_length(aln);

        const auto num_gapopens_between_exons = 
            GetNumGapopensBetweenExons(aln);

        const auto weighted_gaps = 
                   num_gaps 
             + 2 * num_gapopens
             + 4 * num_gapopens_between_exons
             + 4 * num_frameshifts;

        const odds gaps_odds(ungapped_len, weighted_gaps);

        auto     coverage = GetCoverage(aln, ungapped_len);
        auto splices_odds = GetSplicesOdds(aln);
        auto   exons_odds = GetExonsOdds(aln);
        auto matches_odds = GetIdentOdds(aln);
             matches_odds += polya_len;

        if(polya_len > 5) {
            // If we have alignments in both orientations,
            // we may have a polyA in one orinetation and
            // a corresponding false-positive exon in opposite
            // orientation. We want the former, so will weigh
            // existence of polyA_len as slightly more than
            // a consensus splice.
            splices_odds.pos += 1.1;
        }

        const bool defect_free = matches_odds.neg == 0
                              &&    gaps_odds.neg == 0
                              && splices_odds.neg == 0;

        // Given two alignments with equivalent identity, we prefer the
        // one where defects (mismatches and indels) are clustered.
        // 
        // To ascertain that, we'll define identity-like score based 
        // on a longest defect-free matchrun (or diag-run in case of proteins),
        // L/(L+1) with corresponding odds L/1.
        //
        // We clamp L from below at 1, such that 
        // if it wasn't computed (not a spliced-seg), 
        // the odds will be 1/1 (contributing logodds = 0)
        // 
        // We clamp L from above to a fraction of an alignment length because
        // we don't want this to have any effect for high-identity alignments.
        // (e.g. if we have a single-mismatch alignment, then
        // L will vary between [aln_len/2, aln_len), but we don't want
        // the score to be affected.
        const size_t longest_matchrun = GetLongestMatchrunLen(aln);
        const odds matchrun_odds(
                clamp(longest_matchrun, 1, ungapped_len/4.0), 
                1);


        auto ret = 
               0.5 * matches_odds.logodds()  // taking average of these two,
            +  0.5 * matchrun_odds.logodds() // since they are highly correlated
            +  gaps_odds.logodds()
            +  splices_odds.logodds()
            +    exons_odds.logodds()
            +  defect_free * 0.1
            +  as_logodds( coverage == 0 ? 0.5 // logodds==0
                         : clamp(coverage, 0.05, 0.95))
            ;

#if 0
        cerr << "ret:" << ret 
             << " ma" << matches_odds.AsString()
             << " mr" << matchrun_odds.AsString()
             << " g" << gaps_odds.AsString()
             << " s" << splices_odds.AsString()
             << " e" << exons_odds.AsString()
             << " " << (defect_free ? "perfect" : "not-perfect") 
             << (int)round(100 * ret)
             << "\n";
#endif

        return ret;
    }

    ///////////////////////////////////////////////////////////////////////

    double GetSpliceConsensusScore(
            const string& donor, // dinucleotides
            const string& acptr)  const
    {
        const bool have_donor = !(donor == "" || donor == "NN");
        const bool have_acptr = !(acptr == "" || acptr == "NN");

        // note: these values are approximate log-odds
        // and are based on distribution of splice-sites
        // across many taxa.
        //
        // GT-AG occurs in about 97.5% of the time, so 
        // it is the only one with the positive log-odds.
        
        const double x = 
            (have_donor && have_acptr) ?
                 ( donor == "GT" && acptr == "AG" ? 3.75
                 : donor == "AC" && acptr == "AG" ? -4.5
                 : donor == "AT" && acptr == "AC" ? -6.5
                 : donor == "GT" || acptr == "AG" ? -7.0 
                 :                                  -10.0)

            :    ( !have_donor && !have_acptr     ? 0.0
                 : !have_donor  ? (acptr == "AG"  ? 2.0 : -7.0)
                 : !have_acptr  ? (donor == "GT"  ? 2.0 : -7.0)
                 :                                  0.0);

        return x / 3.75;
    };

    ///////////////////////////////////////////////////////////////////////
    // return 0 if can't compute 
    static size_t GetLongestMatchrunLen(const CSeq_align& aln)
    {
        if(!aln.GetSegs().IsSpliced()) {
            return 0;
        }

        size_t max_len = 0;
        size_t curr_len = 0;
        for(const auto& exon : aln.GetSegs().GetSpliced().GetExons()) {
            for(const auto& chunk : exon->GetParts()) {

                if(chunk->IsMatch()) {
                    curr_len += chunk->GetMatch();
                } else if(chunk->IsDiag()) {
                    curr_len += chunk->GetDiag();
                } else {
                    max_len = max(max_len, curr_len);
                    curr_len = 0;
                }
            }
        }

        return max(curr_len, max_len);
    }

    ///////////////////////////////////////////////////////////////////////

    static Int8 GetGapLengthOnRow(
            const CSpliced_exon& exon1,
            const CSpliced_exon& exon2,
            CSeq_align::TDim row)
    {
        const auto r1 = exon1.GetRowSeq_range(row, true);       
        const auto r2 = exon2.GetRowSeq_range(row, true);

        return r1.CombinationWith(r2).GetLength()
              - r1.GetLength()
              - r2.GetLength();
    }

    ///////////////////////////////////////////////////////////////////////

#pragma push_macro("GET_SCORE")
#define GET_SCORE(score_name) \
        double score_name = 0.0; \
        bool have_##score_name = aln.GetNamedScore(#score_name, score_name); \
        (void)score_name; (void)have_##score_name
        // (void)variable is to quiet unused-variable warnings

    ///////////////////////////////////////////////////////////////////////

    static bool isProtSS(const CSeq_align& aln)
    {
        return aln.GetSegs().IsSpliced()
            && aln.GetSegs().GetSpliced().GetProduct_type() ==
               CSpliced_seg::eProduct_type_protein;
    }

    ///////////////////////////////////////////////////////////////////////

    static odds s_GetIdentOdds_protSS(const CSeq_align& aln)
    {
        _ASSERT(isProtSS(aln));

        GET_SCORE(num_ident);
        GET_SCORE(num_mismatch);

        GET_SCORE(num_positives);
        GET_SCORE(num_negatives);

        // Note: for ProSplign spliced-segs these are in terms of NAs, not AAs,
        // JIRA:GP-27664

        const size_t ungapped_len = aln.GetAlignLength(false); // in NAs, not AAs.

        if(!have_num_ident && have_num_mismatch) {
            have_num_ident = true;
            num_ident = ungapped_len - num_mismatch;
        }

        if(!have_num_positives && have_num_negatives) {
            have_num_positives = true;
            num_positives = ungapped_len - num_negatives;
        }

        // GP-9599 - disregarding num_positives if have num_ident.
        // (used to take weighted sum thereof when both are present).

        double identity = have_num_ident     ?     num_ident / ungapped_len
                        : have_num_positives ? num_positives / ungapped_len
                        : 0.5; // log-odds=0.

        if(identity < 0 || identity > 1.0) {
            std::cerr << MSerial_AsnText << aln;
            NCBI_USER_THROW("Identity is outside of [0..1] range - problem with alignment scores.");
        }

        return odds(ungapped_len * identity,
                    ungapped_len * (1 - identity));
    }        

    ///////////////////////////////////////////////////////////////////////

    static odds s_GetIdentOdds_nucSS(const CSeq_align& aln)
    {
        _ASSERT(aln.GetSegs().IsSpliced() && !isProtSS(aln));

        odds res;
        for(const auto& exon : aln.GetSegs().GetSpliced().GetExons()) {
            for(const auto& chunk : exon->GetParts()) {
                if(chunk->IsMatch()) {
                    res.pos += chunk->GetMatch();
                } else if(chunk->IsMismatch()) {
                    res.neg += chunk->GetMismatch();
                }
            }
        }
        return res;
    }

    ///////////////////////////////////////////////////////////////////////

    static odds s_GetIdentOdds_disc(const CSeq_align& aln)
    {
        odds res;

        for(const auto subaln : aln.GetSegs().GetDisc().Get()) {
            res += GetIdentOdds(*subaln);
        }
        return res;
    }

    ///////////////////////////////////////////////////////////////////////

    static odds GetIdentOdds(const CSeq_align& aln)
    {
        if(isProtSS(aln)) {
            return s_GetIdentOdds_protSS(aln);
        }

        GET_SCORE(num_ident);
        GET_SCORE(num_mismatch);

        // Note: if alignment is from tblastn, 
        // num_ident/num_mismatch/num_positives/num_negatives are in AAs,
        // and CSeq_align::GetAlignLength is in AAs.
        //
        // if the alignment is from ProSplign, those are in NAs.

        if(have_num_ident && have_num_mismatch) {
            return odds(num_ident, 
                        num_mismatch);
        }
       
        const auto ungapped_len = aln.GetAlignLength(false);

        if(have_num_ident) {
            return odds(num_ident, 
                        ungapped_len - num_ident);
        }

        if(have_num_mismatch) {
            return odds(ungapped_len - num_mismatch, 
                        num_mismatch);
        }

        GET_SCORE(pct_identity_ungap);

        if(have_pct_identity_ungap) {
            num_ident = pct_identity_ungap * 0.01 * ungapped_len;
            return odds(num_ident, 
                        ungapped_len - num_ident);
        }

        GET_SCORE(pct_identity_gap);

        if(have_pct_identity_gap) {
            const auto gapped_len = aln.GetAlignLength(true);
            num_ident = pct_identity_gap * 0.01 * gapped_len;
            return odds(num_ident, 
                        ungapped_len - num_ident);
        }

        if(aln.GetSegs().IsSpliced() && !isProtSS(aln)) {
            return s_GetIdentOdds_nucSS(aln);
        } else if(aln.GetSegs().IsDisc()) {
            return s_GetIdentOdds_disc(aln);
        }

        cerr << MSerial_AsnText << aln;
        NCBI_USER_THROW("Can't get ident/mismatch count for aln");
    }

    ///////////////////////////////////////////////////////////////////////

    odds GetSplicesOdds(const CSeq_align& aln) const
    {
        odds res;
        if(!aln.GetSegs().IsSpliced()) {
            return res;
        }

        const CSpliced_seg& ss = aln.GetSegs().GetSpliced();

        CConstRef<CSpliced_exon> prev;
        for(const auto& exon : ss.GetExons()) {
            if(!prev) {
                prev = exon;
                continue;
            }
    
            // not checking for abutting, since
            // the scoring function can score a
            // single splice

            res += GetSpliceConsensusScore(
                prev->IsSetDonor_after_exon() 
              ? prev->GetDonor_after_exon().GetBases() : "",

                exon->IsSetAcceptor_before_exon() 
              ? exon->GetAcceptor_before_exon().GetBases() : "");

            prev = exon;
        }

        return res;
    }
   
    ///////////////////////////////////////////////////////////////////////
    /// pos + neg = count of exons.
    /// neg = count of "bad" exons 
    ///          (partial or short or downstream of a long intron)
    odds GetExonsOdds(const CSeq_align& aln) const
    {
        odds res;
        if(!aln.GetSegs().IsSpliced()) {
            return res;
        }

        const CSpliced_seg& ss = aln.GetSegs().GetSpliced();

        CConstRef<CSpliced_exon> prev;
        for(const auto& exon : ss.GetExons()) {
    
            const bool is_terminal = exon == ss.GetExons().front()
                                  || exon == ss.GetExons().back();

            const size_t len = exon->GetRowSeq_range(1, true).GetLength();

            const bool is_poor =
                (exon->IsSetPartial() && exon->GetPartial())
             || (len <= (is_terminal ? 15 : 5))
             || (prev && GetGapLengthOnRow(*prev, *exon, 1) > 65000);

            res += is_poor ? -1 : 1;

            prev = exon;
        }

        return res;
    }

    ///////////////////////////////////////////////////////////////////////

    static size_t GetNumGapopensBetweenExons(const CSeq_align& aln) 
    {
        if(!aln.GetSegs().IsSpliced()) {
            return 0;
        }

        CConstRef<CSpliced_exon> prev;
        size_t n = 0;
        for(const auto& exon : aln.GetSegs().GetSpliced().GetExons()) {
            n += prev && GetGapLengthOnRow(*prev, *exon, 0) != 0;
            prev = exon;
        }
        return n;
    }
 
    ///////////////////////////////////////////////////////////////////////

    static size_t GetPolyA_length(const CSeq_align& aln)
    {
        if(!aln.GetSegs().IsSpliced()) {
            return 0;
        }   

        const CSpliced_seg& ss = aln.GetSegs().GetSpliced();

        return !ss.IsSetPoly_a()         ? 0
             : !ss.IsSetProduct_length() ? 0

             : aln.GetSeqStrand(0) == eNa_strand_minus 
                    ? ss.GetPoly_a()

             : aln.GetSeqStrand(0) == eNa_strand_plus  
                    ? ss.GetProduct_length() - ss.GetPoly_a()

             : 0;  
    }

    ///////////////////////////////////////////////////////////////////////

    // return 0 if can't compute
    static double GetCoverage(const CSeq_align& aln, size_t ung_aln_len)
    {
        GET_SCORE(pct_coverage_hiqual);
        if(have_pct_coverage_hiqual) {
            return pct_coverage_hiqual / 100;
        }

        GET_SCORE(pct_coverage);
        if(have_pct_coverage) {
            return pct_coverage / 100;
        }

        const bool is_prot = isProtSS(aln);
        
        if(   aln.GetSegs().IsSpliced() 
           && aln.GetSegs().GetSpliced().IsSetProduct_length())
        {
            auto len = aln.GetSegs().GetSpliced().GetProduct_length()
                     * (is_prot ? 3 : 1);
            
            return ung_aln_len * 1.0 / len;
        }

        return 0.0;
    }

    ///////////////////////////////////////////////////////////////////////
    //


#pragma pop_macro("GET_SCORE")
};

/////////////////////////////////////////////////////////////////////////////

int NBestPlacement::GetScore(const CSeq_align& aln)
{
    static SAlignmentScoringModel get_score;
    return get_score(aln);
}

/////////////////////////////////////////////////////////////////////////////

void NBestPlacement::Rank(
           CSeq_align_set& sas, 
                score_fn_t score_fn)
{
    if(sas.Get().empty()) {
        return;
    }

    using pair_t = pair<int, CRef<CSeq_align> >;
    vector<pair_t> v;

    for(const auto aln : sas.Set()) {

        _ASSERT(          aln->GetSeq_id(0).Equals(
            sas.Get().front()->GetSeq_id(0)));

        v.emplace_back(score_fn(*aln), aln);
    }

    stable_sort(
           v.begin(), v.end(),
           [](const pair_t a, const pair_t b) {
                return b.first < a.first; // by score, descending
           });
    
    const auto rank1_count = std::count_if(
            v.begin(), v.end(),
            [&v](pair_t p) { return p.first == v.front().first; });

    sas.Set().clear();

    int rank = 1;
    int rank1_index = 1;
    for(const auto p : v) {
        auto aln = p.second;

        if(rank1_count > 1 && p.first == v.front().first) {
            aln->SetNamedScore("rank", 1);
            aln->SetNamedScore("rank1_index", rank1_index++);
            aln->SetNamedScore("rank1_count", (int)rank1_count);
        } else {
            aln->SetNamedScore("rank", rank);
        }

        rank++;

        aln->SetNamedScore("best_placement_score", p.first);

        sas.Set().emplace_back(aln);
    }
}
