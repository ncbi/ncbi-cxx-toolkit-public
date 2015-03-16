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
* Author:  Tom Madden
*
* File Description:
*   Unit test module for ScoreBlk related functions.
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <corelib/metareg.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <blast_objmgr_priv.hpp>

#include <algo/blast/core/ncbi_math.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_encoding.h>

#include "test_objmgr.hpp"

#include <string>
#include <vector>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

BOOST_AUTO_TEST_SUITE(scoreblk)

BOOST_AUTO_TEST_CASE(GetScoreBlockNucl) {
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_both));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastn));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info); 
    SetupQueries(query_v, query_info, &query_blk, 
                 prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_REQUIRE(m->empty());
    }

    CBlastScoringOptions scoring_opts;
    Int2 rv = BlastScoringOptionsNew(kProgram, &scoring_opts);
    BOOST_REQUIRE(rv == 0);
    BlastScoreBlk* sbp;
    Blast_Message* blast_message = NULL;
    Int2 status = BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_opts, 
        kProgram, &sbp, 1.0, &blast_message, &BlastFindMatrixPath);
    BOOST_REQUIRE(status == 0);

    BOOST_REQUIRE_EQUAL(0, (int) sbp->protein_alphabet);
    BOOST_REQUIRE_EQUAL(99, (int) sbp->alphabet_code);
    BOOST_REQUIRE_EQUAL(16, (int) sbp->alphabet_size);
    BOOST_REQUIRE_EQUAL(0, (int) sbp->alphabet_start);
    BOOST_REQUIRE_EQUAL(-3, (int) sbp->loscore);
    BOOST_REQUIRE_EQUAL(1, (int) sbp->hiscore);
    BOOST_REQUIRE_EQUAL(-3, (int) sbp->penalty);
    BOOST_REQUIRE_EQUAL(1, (int) sbp->reward);

    sbp = BlastScoreBlkFree(sbp);
    BOOST_REQUIRE(sbp == NULL);

}

BOOST_AUTO_TEST_CASE(GetScoreBlockProtein) {
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    CSeq_id id("gi|3091");
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(id));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastp));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info); 
    SetupQueries(query_v, query_info, &query_blk, 
                 prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_REQUIRE(m->empty());
    }

    CBlastScoringOptions scoring_opts;
    Int2 rv = BlastScoringOptionsNew(kProgram, &scoring_opts);
    BOOST_REQUIRE(rv == 0);
    BlastScoreBlk* sbp;
    CBlast_Message error_msg;
    Int2 status = BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_opts,
        kProgram, &sbp, 1.0, &error_msg, &BlastFindMatrixPath);
    BOOST_REQUIRE(status == 0);

    BOOST_REQUIRE_EQUAL(1, (int) sbp->protein_alphabet);
    BOOST_REQUIRE_EQUAL(11, (int) sbp->alphabet_code);
    BOOST_REQUIRE_EQUAL(BLASTAA_SIZE, (int) sbp->alphabet_size);
    BOOST_REQUIRE_EQUAL(0, (int) sbp->alphabet_start);
    BOOST_REQUIRE_EQUAL(-4, (int) sbp->loscore);
    BOOST_REQUIRE_EQUAL(11, (int) sbp->hiscore);

    sbp = BlastScoreBlkFree(sbp);
    BOOST_REQUIRE(sbp == NULL);
}

BOOST_AUTO_TEST_CASE(GetScoreBlockPHI) {
    const EBlastProgramType kProgram = eBlastTypePhiBlastp;
    const string kPhiPattern("EVALNAEGWQSSG");
    const string kMatrix("BLOSUM45");
    const int kGapOpenBad = 16; // Unsupported value
    const int kGapOpenGood = 14;// Supported value
    const int kGapExtend = 2;
    const string kErrorMsg("The combination 16 for gap opening cost");
    const double kPhiLambda = 0.199;
    const double kPhiK = 0.040;

    CSeq_id id("gi|3091");
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(id));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;

    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastp));
    opts->SetOptions().SetPHIPattern(kPhiPattern.c_str(), false);

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info); 
    SetupQueries(query_v, query_info, &query_blk, 
                 prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_REQUIRE(m->empty());
    }

    CBlastScoringOptions scoring_opts;
    Int2 rv = BlastScoringOptionsNew(kProgram, &scoring_opts);
    BOOST_REQUIRE(rv == 0);
    sfree(scoring_opts->matrix);
    scoring_opts->matrix = strdup(kMatrix.c_str());
    scoring_opts->gap_open = kGapOpenBad;
    scoring_opts->gap_extend = kGapExtend;

    BlastScoreBlk* sbp;
    Blast_Message* blast_message=NULL;

    Int2 status = 
        BlastSetup_ScoreBlkInit(query_blk, query_info, 
                                scoring_opts,
                                kProgram, &sbp, 1.0, 
                                &blast_message,
                                &BlastFindMatrixPath);

    BOOST_REQUIRE_EQUAL(-1, (int)status);
    sbp = BlastScoreBlkFree(sbp);
    BOOST_REQUIRE(sbp == NULL);
    
    BOOST_REQUIRE(!strncmp(kErrorMsg.c_str(), blast_message->message,
                           kErrorMsg.size()));
    // blast_message will be reused in the next call, so this message 
    // must be freed.
    Blast_MessageFree(blast_message);

    scoring_opts->gap_open = kGapOpenGood;
    status = 
        BlastSetup_ScoreBlkInit(query_blk, query_info, 
                                scoring_opts,
                                kProgram, &sbp, 1.0, 
                                &blast_message,
                                &BlastFindMatrixPath);
    BOOST_REQUIRE_EQUAL(0, (int) status);
    BOOST_REQUIRE(sbp->kbp_std != sbp->kbp_gap_std);
    BOOST_REQUIRE(sbp->kbp == sbp->kbp_std);
    BOOST_REQUIRE(sbp->kbp_gap == sbp->kbp_gap_std);
    BOOST_REQUIRE(sbp->kbp_gap[0]->Lambda == kPhiLambda);
    BOOST_REQUIRE(sbp->kbp_gap[0]->K == kPhiK);
    BOOST_REQUIRE(sbp->kbp_gap[0]->H > 0);
    BOOST_REQUIRE(sbp->kbp[0]->Lambda == kPhiLambda);
    BOOST_REQUIRE(sbp->kbp[0]->K == kPhiK);
    BOOST_REQUIRE(sbp->kbp[0]->H > 0);

    sbp = BlastScoreBlkFree(sbp);
    BOOST_REQUIRE(sbp == NULL);
}

BOOST_AUTO_TEST_CASE(GetScoreBlockForFullyMaskedProtein) {
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    CSeq_id id("gi|3091");
    const int start = 0;
    const int stop = 27;
    pair<TSeqPos, TSeqPos> range(start, stop);
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(id, range));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastp));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info); 
    SetupQueries(query_v, query_info, &query_blk, 
                 prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_REQUIRE(m->empty());
    }

    CBlastScoringOptions scoring_opts;
    Int2 rv = BlastScoringOptionsNew(kProgram, &scoring_opts);
    BOOST_REQUIRE(rv == 0);

    BlastSeqLoc *loc = BlastSeqLocNew(NULL, start, stop);
    BlastMaskLoc* filter_maskloc = BlastMaskLocNew(1);
    filter_maskloc->seqloc_array[0] = loc;

    BlastSetUp_MaskQuery(query_blk, query_info, filter_maskloc, kProgram);
    filter_maskloc = BlastMaskLocFree(filter_maskloc);
    BOOST_REQUIRE(filter_maskloc == NULL);


    BlastScoreBlk* sbp;
    Blast_Message* blast_message = NULL;
    Int2 status = BlastSetup_ScoreBlkInit(query_blk, query_info, 
                                          scoring_opts, kProgram, &sbp, 
                                          1.0, &blast_message,
                                          &BlastFindMatrixPath);

    // Note that errors will come in the Blast_Message structure
    BOOST_REQUIRE_EQUAL(0, (int) status);
    BOOST_REQUIRE(blast_message != NULL);

    BOOST_REQUIRE_EQUAL(1, (int) sbp->protein_alphabet);
    BOOST_REQUIRE_EQUAL(11, (int) sbp->alphabet_code);
    BOOST_REQUIRE_EQUAL(BLASTAA_SIZE, (int) sbp->alphabet_size);
    BOOST_REQUIRE_EQUAL(0, (int) sbp->alphabet_start);
    BOOST_REQUIRE_EQUAL(-4, (int) sbp->loscore);
    BOOST_REQUIRE_EQUAL(11, (int) sbp->hiscore);

    blast_message = Blast_MessageFree(blast_message);
    BOOST_REQUIRE(blast_message == NULL);
    sbp = BlastScoreBlkFree(sbp);
    BOOST_REQUIRE(sbp == NULL);
}

BOOST_AUTO_TEST_CASE(BlastResFreqStdCompProteinTest) {

    BlastScoreBlk sbp;
    sbp.alphabet_code = 11;
    sbp.protein_alphabet = TRUE;
    sbp.alphabet_start = 0;
    sbp.alphabet_size = 26;

    Blast_ResFreq* stdrfp = Blast_ResFreqNew(&sbp);
    Blast_ResFreqStdComp(&sbp, stdrfp);

    BOOST_REQUIRE_EQUAL(11, (int) stdrfp->alphabet_code);
    // All frequencies multiplied by 100000 and truncated to nearest integer
    // to avoid rounding error of floating points.
    // some ambiguity codes, should be zero.
    BOOST_REQUIRE_EQUAL(0, (int)BLAST_Nint(100000*stdrfp->prob[2])); // Asp or Asn
    BOOST_REQUIRE_EQUAL(0, (int)BLAST_Nint(100000*stdrfp->prob[21]));  // X
    // some "real" residues we use. 
    BOOST_REQUIRE_EQUAL(3856, (int)BLAST_Nint(100000*stdrfp->prob[6])); 
    BOOST_REQUIRE_EQUAL(2243, (int)BLAST_Nint(100000*stdrfp->prob[12])); 
    BOOST_REQUIRE_EQUAL(3216, (int)BLAST_Nint(100000*stdrfp->prob[22])); 

    stdrfp = Blast_ResFreqFree(stdrfp);
    BOOST_REQUIRE(stdrfp == NULL);
}

BOOST_AUTO_TEST_CASE(BlastResFreqStdCompNucleotideTest) {

    BlastScoreBlk sbp;
    sbp.alphabet_code = 99;
    sbp.protein_alphabet = FALSE;
    sbp.alphabet_start = 0;
    sbp.alphabet_size = 16;

    Blast_ResFreq* stdrfp = Blast_ResFreqNew(&sbp);
    Blast_ResFreqStdComp(&sbp, stdrfp);

    BOOST_REQUIRE_EQUAL(99, (int) stdrfp->alphabet_code);
    const int num_real_bases = 4;
    // Multiplied by 100 and truncated to avoid rounding error of floating point.
    for (int index=0; index<num_real_bases; index++) // A,C,G,T
        BOOST_REQUIRE_EQUAL(25, (int)(100*stdrfp->prob[index])); 

    for (int index=num_real_bases; index<sbp.alphabet_size; index++) // ambiguity codes, are all zero.
        BOOST_REQUIRE_EQUAL(0, (int)(100*stdrfp->prob[index])); 

    stdrfp = Blast_ResFreqFree(stdrfp);
    BOOST_REQUIRE(stdrfp == NULL);
}

BOOST_AUTO_TEST_CASE(EqualRewardPenaltyLHtoK) 
{
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    BlastScoringOptions* score_opts = NULL;
    BlastScoringOptionsNew(kProgram, &score_opts);
    score_opts->reward = 2;
    score_opts->penalty = -2;

    BlastScoreBlk* sbp = BlastScoreBlkNew(BLASTNA_SEQ_CODE, 1);
    Blast_ScoreBlkMatrixInit(kProgram, score_opts, sbp, NULL); 
    Blast_ScoreBlkKbpIdealCalc(sbp);

    BOOST_REQUIRE(fabs(sbp->kbp_ideal->K - 1.0/3) < 1e-6);
    BlastScoreBlkFree(sbp);
    BlastScoringOptionsFree(score_opts);
}

BOOST_AUTO_TEST_CASE(NuclGappedCalc)
{
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    CBlastScoringOptions score_opts;
    BlastScoringOptionsNew(kProgram, &score_opts);
    score_opts->reward = 1;
    score_opts->penalty = -2;
    score_opts->gap_open = 3;
    score_opts->gap_extend = 1;

    CBlastScoreBlk sbp(BlastScoreBlkNew(BLASTNA_SEQ_CODE, 1));
    Blast_ScoreBlkMatrixInit(kProgram, score_opts, sbp, NULL);
    Blast_ScoreBlkKbpIdealCalc(sbp);
    
    Blast_KarlinBlk* kbp = Blast_KarlinBlkNew();
    CBlast_Message error_msg;
    Int2 status = 0;
    status = 
        Blast_KarlinBlkNuclGappedCalc(kbp, score_opts->gap_open,
            score_opts->gap_extend, score_opts->reward,
            score_opts->penalty, sbp->kbp_ideal, 
            &(sbp->round_down), &error_msg);
    
    BOOST_REQUIRE_EQUAL(0, (int) status);
    BOOST_REQUIRE_EQUAL(false, (bool) sbp->round_down);
    BOOST_REQUIRE_CLOSE(1.32, kbp->Lambda, 0.001);
    BOOST_REQUIRE_CLOSE(0.57, kbp->K, 0.001);
    BOOST_REQUIRE_CLOSE(-0.562, kbp->logK, 0.1);
    BOOST_REQUIRE(error_msg.Get() == NULL);
    error_msg.Reset();

    // Check values of alpha and beta parameters
    double alpha, beta;
    Blast_GetNuclAlphaBeta(score_opts->reward, score_opts->penalty,
                           score_opts->gap_open, score_opts->gap_extend,
                           sbp->kbp_ideal, TRUE, &alpha, &beta);
    BOOST_REQUIRE_CLOSE(1.3, alpha, 0.001);
    BOOST_REQUIRE_CLOSE(-1.0, beta, 0.001);

    // Check gap costs for which Karlin-Altschul paramters are copied
    // from the ungapped block.
    score_opts->gap_open = 4;
    score_opts->gap_extend = 2;

    status = 
        Blast_KarlinBlkNuclGappedCalc(kbp, score_opts->gap_open,
            score_opts->gap_extend, score_opts->reward,
            score_opts->penalty, sbp->kbp_ideal,
            &(sbp->round_down), &error_msg);
    BOOST_REQUIRE_EQUAL(0, (int) status);
    BOOST_REQUIRE_EQUAL(false, (bool) sbp->round_down);
    BOOST_REQUIRE_EQUAL(sbp->kbp_ideal->Lambda, kbp->Lambda);
    BOOST_REQUIRE_EQUAL(sbp->kbp_ideal->K, kbp->K);
    BOOST_REQUIRE_EQUAL(sbp->kbp_ideal->logK, kbp->logK);
    BOOST_REQUIRE(error_msg.Get() == NULL);
    error_msg.Reset();

    Blast_GetNuclAlphaBeta(score_opts->reward, score_opts->penalty,
                           score_opts->gap_open, score_opts->gap_extend,
                           sbp->kbp_ideal, TRUE, &alpha, &beta);
    BOOST_REQUIRE_CLOSE(sbp->kbp_ideal->Lambda/sbp->kbp_ideal->H,
                                 alpha, 1e-10);
    BOOST_REQUIRE_EQUAL(0.0, beta);

    // Check for scaled up values.
    score_opts->reward = 10;
    score_opts->penalty = -20;
    score_opts->gap_open = 30;
    score_opts->gap_extend = 10;

    status = 
        Blast_KarlinBlkNuclGappedCalc(kbp, score_opts->gap_open,
            score_opts->gap_extend, score_opts->reward,
            score_opts->penalty, sbp->kbp_ideal, 
            &(sbp->round_down), &error_msg);
    
    BOOST_REQUIRE_EQUAL(0, (int) status);
    BOOST_REQUIRE_EQUAL(false, (bool) sbp->round_down);
    BOOST_REQUIRE_CLOSE(0.132, kbp->Lambda, 0.001);
    BOOST_REQUIRE_CLOSE(0.57, kbp->K, 0.001);
    BOOST_REQUIRE_CLOSE(-0.562, kbp->logK, 0.1);

    // For this set of values the score needs to be rounded down, mostly checking
    // here that the round_down bool is set properly
    score_opts->reward = 2;
    score_opts->penalty = -7;
    score_opts->gap_open = 4;
    score_opts->gap_extend = 2;

    status = 
        Blast_KarlinBlkNuclGappedCalc(kbp, score_opts->gap_open,
            score_opts->gap_extend, score_opts->reward,
            score_opts->penalty, sbp->kbp_ideal,
            &(sbp->round_down), &error_msg);
    BOOST_REQUIRE_EQUAL(0, (int) status);
    BOOST_REQUIRE_EQUAL(true, (bool) sbp->round_down);
    BOOST_REQUIRE_CLOSE(0.675, kbp->Lambda, 0.001);
    BOOST_REQUIRE_CLOSE(0.62, kbp->K, 0.001);
    BOOST_REQUIRE_CLOSE(-0.478036, kbp->logK, 0.001);
    BOOST_REQUIRE(error_msg.Get() == NULL);
    error_msg.Reset();

    // Check invalid gap costs that were permitted owing to a bug.
    score_opts->reward = 4;
    score_opts->penalty = -5;
    score_opts->gap_open = 3;
    score_opts->gap_extend = 2;

    status = 
        Blast_KarlinBlkNuclGappedCalc(kbp, score_opts->gap_open,
            score_opts->gap_extend, score_opts->reward,
            score_opts->penalty, sbp->kbp_ideal,
            &(sbp->round_down), &error_msg);
    BOOST_REQUIRE_EQUAL(1, (int) status);
    BOOST_REQUIRE(!strncmp("Gap existence and extension values 3 and 2 are not supported for substitution scores 4 and -5", 
                           error_msg->message, 90));
    error_msg.Reset();

    // Check invalid gap costs 
    score_opts->reward = 1;
    score_opts->penalty = -2;
    score_opts->gap_open = 1;
    score_opts->gap_extend = 3;

    status = 
        Blast_KarlinBlkNuclGappedCalc(kbp, score_opts->gap_open,
            score_opts->gap_extend, score_opts->reward,
            score_opts->penalty, sbp->kbp_ideal,
            &(sbp->round_down), &error_msg);
    BOOST_REQUIRE_EQUAL(1, (int) status);
    BOOST_REQUIRE(!strncmp("Gap existence and extension values 1 and 3 are not supported for substitution scores 1 and -2", 
                           error_msg->message, 90));
    error_msg.Reset();

    // Alpha and beta can be returned even for unsupported gap costs, 
    // because this function should work for an ungapped search.
    Blast_GetNuclAlphaBeta(score_opts->reward, score_opts->penalty,
                           score_opts->gap_open, score_opts->gap_extend,
                           sbp->kbp_ideal, TRUE, &alpha, &beta);
    BOOST_REQUIRE_CLOSE(sbp->kbp_ideal->Lambda/sbp->kbp_ideal->H,
                                 alpha, 1e-10);
    BOOST_REQUIRE_EQUAL(0.0, beta);
    
    // Check invalid substitution scores
    score_opts->reward = 2;
    score_opts->penalty = -1;
    status = 
        Blast_KarlinBlkNuclGappedCalc(kbp, score_opts->gap_open,
            score_opts->gap_extend, score_opts->reward,
            score_opts->penalty, sbp->kbp_ideal,
            &(sbp->round_down), &error_msg);
    BOOST_REQUIRE_EQUAL(-1, (int) status);
    BOOST_REQUIRE(!strcmp("Substitution scores 2 and -1 are not supported",
                           error_msg->message));
    error_msg.Reset();
    // Alpha and beta would still be returned in this case as for an 
    // ungapped search.
    Blast_GetNuclAlphaBeta(score_opts->reward, score_opts->penalty,
                           score_opts->gap_open, score_opts->gap_extend,
                           sbp->kbp_ideal, TRUE, &alpha, &beta);
    BOOST_REQUIRE_CLOSE(sbp->kbp_ideal->Lambda/sbp->kbp_ideal->H,
                                 alpha, 1e-10);
    BOOST_REQUIRE_EQUAL(0.0, beta);

    // Check alpha and beta for an ungapped search, with a different 
    // pair of substitution scores.
    score_opts->penalty = -3;
    Blast_GetNuclAlphaBeta(score_opts->reward, score_opts->penalty,
                           0, 0, sbp->kbp_ideal, FALSE, &alpha, &beta);

    BOOST_REQUIRE_CLOSE(sbp->kbp_ideal->Lambda/sbp->kbp_ideal->H,
                                 alpha, 1e-10);
    BOOST_REQUIRE_EQUAL(-2.0, beta);

    kbp = Blast_KarlinBlkFree(kbp);
    BOOST_REQUIRE(kbp == NULL);
}

BOOST_AUTO_TEST_SUITE_END()
