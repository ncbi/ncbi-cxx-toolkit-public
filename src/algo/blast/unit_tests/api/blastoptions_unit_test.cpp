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
 * Authors: Christiam Camacho
 *
 */

/** @file blastoptions_unit_test.cpp
 * Unit tests for the BLAST options
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include "blast_setup.hpp"
#include "blast_objmgr_priv.hpp"
#include <algo/blast/core/blast_setup.h>
#include "test_objmgr.hpp"
#include <algo/blast/core/hspfilter_besthit.h>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

BOOST_AUTO_TEST_SUITE(blastoptions)

BOOST_AUTO_TEST_CASE(TestTasksDefinitionsAndDocumentation)
{
    set<string> tasks = CBlastOptionsFactory::GetTasks();
    ITERATE(set<string>, itr, tasks) {
        string doc = CBlastOptionsFactory::GetDocumentation(*itr);
        BOOST_CHECK(doc != "Unknown task");

        CRef<CBlastOptionsHandle> opt;
        BOOST_CHECK_NO_THROW(opt.Reset(CBlastOptionsFactory::CreateTask(*itr)));
    }
}

BOOST_AUTO_TEST_CASE( RemoteOptionsTest )
{
     CBlastOptions opts(CBlastOptions::eRemote);
     BOOST_CHECK_NO_THROW(opts.SetMaskAtHash());
     BOOST_CHECK_NO_THROW(opts.SetDustFiltering());
     BOOST_CHECK_NO_THROW(opts.SetSegFiltering());
     BOOST_CHECK_NO_THROW(opts.SetRepeatFiltering());
     BOOST_CHECK_NO_THROW(opts.SetRepeatFilteringDB("repeat/repeat_9606"));
     BOOST_CHECK_NO_THROW(opts.SetFilterString("m L", false)); /* NCBI_FAKE_WARNING */
}

BOOST_AUTO_TEST_CASE( BogusProgramWithCreate )
{
    CRef<CBlastOptionsHandle> opts;
    BOOST_CHECK_THROW(CBlastOptionsFactory::Create(eBlastNotSet),
                      CBlastException);
}

BOOST_AUTO_TEST_CASE( UnifiedPOptionsTest )
{
    CBlastOptions opts;
    
    BOOST_CHECK_EQUAL(opts.GetUnifiedP(), 0);
    opts.SetUnifiedP(1);
    BOOST_CHECK_EQUAL(opts.GetUnifiedP(), 1);
    opts.SetUnifiedP();
    BOOST_CHECK_EQUAL(opts.GetUnifiedP(), 0);
}

BOOST_AUTO_TEST_CASE( GetSuggestedThresholdTest )
{
    Int2 status=0;
    double threshold;

    const int kThresholdSentinel = -33;

    threshold = kThresholdSentinel;
    status = BLAST_GetSuggestedThreshold(eBlastTypeBlastn, "BLOSUM62", &threshold);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(kThresholdSentinel, (int) threshold);

    status = BLAST_GetSuggestedThreshold(eBlastTypeBlastp, NULL, &threshold);
    BOOST_CHECK_EQUAL(BLASTERR_INVALIDPARAM, (int) status);
     
    status = BLAST_GetSuggestedThreshold(eBlastTypeBlastp, "BLOSUM62", &threshold);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(11, (int) threshold);
     
    status = BLAST_GetSuggestedThreshold(eBlastTypeBlastx, "BLOSUM62", &threshold);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(12, (int) threshold);
     
    status = BLAST_GetSuggestedThreshold(eBlastTypeTblastn, "BLOSUM62", &threshold);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(13, (int) threshold);
     
    status = BLAST_GetSuggestedThreshold(eBlastTypeTblastx, "BLOSUM62", &threshold);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(13, (int) threshold);
     
    status = BLAST_GetSuggestedThreshold(eBlastTypeBlastp, "PAM30", &threshold);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(16, (int) threshold);
     
    status = BLAST_GetSuggestedThreshold(eBlastTypeBlastp, "PAM140", &threshold);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(11, (int) threshold);
}

BOOST_AUTO_TEST_CASE( GetSuggestedWindowSizeTest )
{
    Int2 status=0;
    Int4 window_size;

    status = BLAST_GetSuggestedWindowSize(eBlastTypeBlastn, "BLOSUM62", &window_size);
    BOOST_CHECK_EQUAL(0, (int) status);

    status = BLAST_GetSuggestedWindowSize(eBlastTypeBlastp, NULL, &window_size);
    BOOST_CHECK_EQUAL(BLASTERR_INVALIDPARAM, (int) status);

    status = BLAST_GetSuggestedWindowSize(eBlastTypeBlastp, "BLOSUM62", &window_size);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(40, window_size);

    status = BLAST_GetSuggestedWindowSize(eBlastTypeBlastx, "BLOSUM62", &window_size);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(40, window_size);

    status = BLAST_GetSuggestedWindowSize(eBlastTypeBlastp, "BLOSUM80", &window_size);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(25, window_size);

    status = BLAST_GetSuggestedWindowSize(eBlastTypeBlastp, "PAM140", &window_size);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(40, window_size);
}

BOOST_AUTO_TEST_CASE( GetProteinGapExistenceExtendParamsTest )
{
    Int2 status=0;
    Int4 existence=0, extension=0;

    status = BLAST_GetProteinGapExistenceExtendParams(NULL, &existence, &extension);
    BOOST_CHECK_EQUAL(-1, (int) status);

    status = BLAST_GetProteinGapExistenceExtendParams("BLOSUM62", &existence, &extension);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(11, existence);
    BOOST_CHECK_EQUAL(1, extension);

    status = BLAST_GetProteinGapExistenceExtendParams("BLOSUM80", &existence, &extension);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(10, existence);
    BOOST_CHECK_EQUAL(1, extension);

    status = BLAST_GetProteinGapExistenceExtendParams("PAM250", &existence, &extension);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(14, existence);
    BOOST_CHECK_EQUAL(2, extension);
}

BOOST_AUTO_TEST_CASE( GetNucleotideGapExistenceExtendParamsTest )
{
    Int2 status=0;
    int existence=-1, extension=-1;
    int reward=0, penalty=0;

    reward = 0;
    penalty = 3;
    status = BLAST_GetNucleotideGapExistenceExtendParams(reward, penalty, &existence, &extension);
    BOOST_CHECK_EQUAL(-1, (int) status);

    /* megablast linear values. */
    reward = 1;
    penalty = -3;
    existence = 0;
    extension = 0;
    status = BLAST_GetNucleotideGapExistenceExtendParams(reward, penalty, &existence, &extension);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(0, existence);
    BOOST_CHECK_EQUAL(0, extension);

    reward = 1;
    penalty = -3;
    existence = -1;
    extension = -1;
    status = BLAST_GetNucleotideGapExistenceExtendParams(reward, penalty, &existence, &extension);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(2, existence);
    BOOST_CHECK_EQUAL(2, extension);

    reward = 2;
    penalty = -5;
    existence = -1;
    extension = -1;
    status = BLAST_GetNucleotideGapExistenceExtendParams(reward, penalty, &existence, &extension);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(4, existence);
    BOOST_CHECK_EQUAL(4, extension);

    reward = 1;
    penalty = -2;
    existence = -1;
    extension = -1;
    status = BLAST_GetNucleotideGapExistenceExtendParams(reward, penalty, &existence, &extension);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK_EQUAL(2, existence);
    BOOST_CHECK_EQUAL(2, extension);
}

BOOST_AUTO_TEST_CASE( FilterSetUpOptionsDustTest )
{
    SBlastFilterOptions* filter_options;

    Int2 status = SBlastFilterOptionsNew(&filter_options, eDust);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK(filter_options);
    BOOST_CHECK(filter_options->dustOptions);
    BOOST_CHECK(filter_options->segOptions == NULL);
    filter_options = SBlastFilterOptionsFree(filter_options);
}

BOOST_AUTO_TEST_CASE( FilterSetUpOptionsSegTest )
{
    SBlastFilterOptions* filter_options;

    Int2 status = SBlastFilterOptionsNew(&filter_options, eSeg);
    BOOST_CHECK_EQUAL(0, (int) status);
    BOOST_CHECK(filter_options);
    BOOST_CHECK(filter_options->dustOptions == NULL);
    BOOST_CHECK(filter_options->segOptions);
    filter_options = SBlastFilterOptionsFree(filter_options);
}

BOOST_AUTO_TEST_CASE( FilterSetUpOptionsNULLInput )
{
    Int2 status = SBlastFilterOptionsNew(NULL, eSeg);
    BOOST_CHECK_EQUAL(1, (int) status);
}

BOOST_AUTO_TEST_CASE( OptionsFreeNULLInput )
{
    SBlastFilterOptionsFree(NULL);
    BlastQuerySetUpOptionsFree(NULL);
    BlastInitialWordOptionsFree(NULL);
    BlastExtensionOptionsFree(NULL);
    BlastScoringOptionsFree(NULL);
    BlastEffectiveLengthsOptionsFree(NULL);
    LookupTableOptionsFree(NULL);
    BlastHitSavingOptionsFree(NULL);
    PSIBlastOptionsFree(NULL);
    BlastDatabaseOptionsFree(NULL);
}

// FIXME: good up to here
static void
s_FillSearchSpace(BlastQueryInfo *query_info, Int8 searchsp)
{
    for (int i = query_info->first_context;
                i <= query_info->last_context; i++) {
        if (query_info->contexts[i].query_length)
            query_info->contexts[i].eff_searchsp = searchsp;
    }
}

// A helper function to setup the initial word parameters.
static BlastInitialWordParameters* 
s_GetInitialWordParameters(EBlastProgramType program_number, 
                           BLAST_SequenceBlk* query_blk, 
                           BlastQueryInfo* query_info, 
                           BlastScoreBlk* sbp, 
                           const BlastInitialWordOptions* word_options, 
                           int subject_length, 
                           const BlastHitSavingParameters* hit_params)
{
   BlastInitialWordParameters* word_params = NULL;
   LookupTableWrap* lookup_wrap = NULL;
   LookupTableOptions* lookup_options = NULL;
   BlastSeqLoc* blast_seq_loc = BlastSeqLocNew(NULL, 0, query_info->contexts[0].query_length-1);
   QuerySetUpOptions* query_options = NULL;
   BlastQuerySetUpOptionsNew(&query_options);

   LookupTableOptionsNew(program_number, &lookup_options);
   LookupTableWrapInit(query_blk, lookup_options, query_options, blast_seq_loc,
                       sbp, &lookup_wrap, NULL, NULL);
   BlastInitialWordParametersNew(program_number, word_options, hit_params,
      lookup_wrap, sbp, query_info, subject_length, &word_params);

   blast_seq_loc = BlastSeqLocFree(blast_seq_loc);
   lookup_wrap = LookupTableWrapFree(lookup_wrap);
   lookup_options = LookupTableOptionsFree(lookup_options);
   query_options = BlastQuerySetUpOptionsFree(query_options);

   return word_params;
}

BOOST_AUTO_TEST_CASE( testCalcLinkHSPCutoffs )
{
    const EBlastProgramType kBlastProgram = eBlastTypeBlastp;
    const Int4 kAvgSubjectLength = 335;
    const Int4 kSpecificSubjectLength = 186;
    const Int8 kDbLength = 703698559;
    CSeq_id qid("gi|129295");
    auto_ptr<SSeqLoc> qsl(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    CSeq_id sid("gi|129296");
    auto_ptr<SSeqLoc> ssl(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    CBl2Seq blaster(*qsl, *ssl, eBlastp);

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;

    const CBlastOptions& kOpts = blaster.GetOptionsHandle().GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(const_cast<TSeqLocVector&>(blaster.GetQueries()), 
                   prog, strand_opt, &query_info);
    SetupQueries(const_cast<TSeqLocVector&>(blaster.GetQueries()), 
                 query_info, &query_blk, prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_CHECK(m->empty());
    }

    BlastScoringOptions* scoring_options;
    BlastScoringOptionsNew(kBlastProgram, &scoring_options);
    BLAST_FillScoringOptions(scoring_options, kBlastProgram, FALSE, 0, 0,
          "BLOSUM62", -1, -1);
    scoring_options->gapped_calculation = FALSE;
   
    BlastScoreBlk* sbp;
    Blast_Message* blast_message = NULL;
    BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_options, 
                             kBlastProgram, &sbp, 1.0, &blast_message,
                             &BlastFindMatrixPath);

    s_FillSearchSpace(query_info, 98483910471LL);

    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options,
                             scoring_options->gapped_calculation);

    BlastHitSavingOptions* hit_options;
    BlastHitSavingOptionsNew(kBlastProgram, &hit_options,
                             scoring_options->gapped_calculation);
    BOOST_CHECK(hit_options->do_sum_stats);

    BlastHitSavingParameters* hit_params;
    BlastHitSavingParametersNew(kBlastProgram, hit_options, sbp, query_info, kAvgSubjectLength, 2, &hit_params);
    BlastLinkHSPParameters* link_hsp_params = hit_params->link_hsp_params;

    BlastInitialWordOptions* word_options;
    BlastInitialWordOptionsNew(kBlastProgram, &word_options);

    BlastInitialWordParameters* word_params = s_GetInitialWordParameters(kBlastProgram, query_blk,
          query_info, sbp, word_options, kAvgSubjectLength, hit_params);

    CalculateLinkHSPCutoffs(kBlastProgram, query_info, sbp, 
                            link_hsp_params, word_params, 
                            kDbLength, kSpecificSubjectLength);

    BOOST_CHECK_EQUAL(36, link_hsp_params->cutoff_big_gap);
    BOOST_CHECK_EQUAL(41, link_hsp_params->cutoff_small_gap); 

    sbp = BlastScoreBlkFree(sbp);
    ext_options = BlastExtensionOptionsFree(ext_options);
    scoring_options = BlastScoringOptionsFree(scoring_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    word_params = BlastInitialWordParametersFree(word_params);
    word_options = BlastInitialWordOptionsFree(word_options);
    return;
}

static BlastScoreBlk*
s_FillScoreBlkWithBadKbp(BlastQueryInfo* query_info) {
  
    BOOST_REQUIRE(query_info);
    BlastScoreBlk* sbp = BlastScoreBlkNew(BLASTAA_SEQ_CODE, 2);

    sbp->kbp = sbp->kbp_std;

    Blast_KarlinBlk* kbp_bad = Blast_KarlinBlkNew();
    kbp_bad->Lambda = -1.0;
    kbp_bad->K = -1.0;
    kbp_bad->logK = -1.0;
    kbp_bad->H = -1.0;
    sbp->kbp[0] = kbp_bad;
    query_info->contexts[0].is_valid = FALSE;

    Blast_KarlinBlk* kbp_good = Blast_KarlinBlkNew();
    kbp_good->Lambda = 1.1;
    kbp_good->K = 1.1;
    kbp_good->logK = 0.1;
    kbp_good->H = 1.1;
    sbp->kbp[1] = kbp_good;
    query_info->contexts[1].is_valid = TRUE;

    return sbp;
}

BOOST_AUTO_TEST_CASE( testBadKbpForLinkHSPCutoffs )
{
    const EBlastProgramType kBlastProgram = eBlastTypeTblastx;
    const Int4 kAvgSubjectLength = 335;
    const Int4 kSpecificSubjectLength = 186;
    const Int8 kDbLength = 703698559;
    const bool kIsGapped = true;


    CBlastQueryInfo query_info(BlastQueryInfoNew(kBlastProgram, 1));
    query_info->first_context = 0;
    query_info->last_context = 1;
    sfree(query_info->contexts);
    query_info->contexts = (BlastContextInfo*) calloc(2, sizeof(BlastContextInfo));
    query_info->contexts[query_info->last_context].query_offset = 300;
    query_info->contexts[query_info->last_context].query_length = 300;

    BlastScoreBlk* sbp = s_FillScoreBlkWithBadKbp(query_info);
 
    s_FillSearchSpace(query_info, 98483910471LL);

    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options, kIsGapped);

    BlastHitSavingOptions* hit_options;
    BlastHitSavingOptionsNew(kBlastProgram, &hit_options,
                             kIsGapped);
    BOOST_CHECK(hit_options->do_sum_stats);

    BlastHitSavingParameters* hit_params;
    BlastHitSavingParametersNew(kBlastProgram, hit_options, sbp, query_info, kAvgSubjectLength, 0, &hit_params);
    BlastLinkHSPParameters* link_hsp_params = hit_params->link_hsp_params;

    BlastInitialWordParameters word_params;
    word_params.cutoff_score_min = 30;

    CalculateLinkHSPCutoffs(kBlastProgram, query_info, sbp,
                            link_hsp_params, &word_params, 
                            kDbLength, kSpecificSubjectLength);

    BOOST_CHECK_EQUAL(11, link_hsp_params->cutoff_big_gap);
    BOOST_CHECK_EQUAL(0, link_hsp_params->cutoff_small_gap); 

    sbp = BlastScoreBlkFree(sbp);
    ext_options = BlastExtensionOptionsFree(ext_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    return;
}

BOOST_AUTO_TEST_CASE( testCalcLinkHSPCutoffsSmallDB )
{
    const EBlastProgramType kBlastProgram = eBlastTypeBlastp;
    const Int4 kAvgSubjectLength = 316;
    const Int4 kSpecificSubjectLength = 21;
    const Int8 kDbLength = 1358990;
    CSeq_id qid("gi|129295");
    auto_ptr<SSeqLoc> qsl(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    CSeq_id sid("gi|129296");
    auto_ptr<SSeqLoc> ssl(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    CBl2Seq blaster(*qsl, *ssl, eBlastp);

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;

    const CBlastOptions& kOpts = blaster.GetOptionsHandle().GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(const_cast<TSeqLocVector&>(blaster.GetQueries()), 
                   prog, strand_opt, &query_info);
    SetupQueries(const_cast<TSeqLocVector&>(blaster.GetQueries()), 
                 query_info, &query_blk, prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_CHECK(m->empty());
    }

    BlastScoringOptions* scoring_options;
    BlastScoringOptionsNew(kBlastProgram, &scoring_options);
    BLAST_FillScoringOptions(scoring_options, kBlastProgram, FALSE, 0, 0,
          "BLOSUM62", -1, -1);
    scoring_options->gapped_calculation = FALSE;
   
    BlastScoreBlk* sbp = NULL;
    Blast_Message* blast_message = NULL;
    BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_options, 
                            kBlastProgram, &sbp, 1.0, &blast_message,
                            &BlastFindMatrixPath);

    s_FillSearchSpace(query_info, 218039195);

    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options,
                             scoring_options->gapped_calculation);

    BlastHitSavingOptions* hit_options;
    BlastHitSavingOptionsNew(kBlastProgram, &hit_options,
                             scoring_options->gapped_calculation);

    BlastHitSavingParameters* hit_params;
    BlastHitSavingParametersNew(kBlastProgram, hit_options, sbp, query_info, 0, 0, &hit_params);
    BlastLinkHSPParameters* link_hsp_params = hit_params->link_hsp_params;

    BlastInitialWordOptions* word_options;
    BlastInitialWordOptionsNew(kBlastProgram, &word_options);

    BlastInitialWordParameters* word_params = s_GetInitialWordParameters(kBlastProgram, query_blk,
          query_info, sbp, word_options, kAvgSubjectLength, hit_params);


    CalculateLinkHSPCutoffs(kBlastProgram, query_info, sbp, 
                            link_hsp_params, word_params,
                            kDbLength, kSpecificSubjectLength);

    BOOST_CHECK_EQUAL(21, link_hsp_params->cutoff_big_gap);
    BOOST_CHECK_EQUAL(0, link_hsp_params->cutoff_small_gap);

    sbp = BlastScoreBlkFree(sbp);
    ext_options = BlastExtensionOptionsFree(ext_options);
    scoring_options = BlastScoringOptionsFree(scoring_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    word_params = BlastInitialWordParametersFree(word_params);
    word_options = BlastInitialWordOptionsFree(word_options);
    return;
}

BOOST_AUTO_TEST_CASE( testCalcLinkHSPResetGapProb )
{
    const EBlastProgramType kBlastProgram = eBlastTypeBlastp;
    const Int4 kAvgSubjectLength = 335;
    const Int4 kSpecificSubjectLength = 186;
    const Int8 kDbLength = 703698559;
    CSeq_id qid("gi|129295");
    auto_ptr<SSeqLoc> qsl(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    CSeq_id sid("gi|129296");
    auto_ptr<SSeqLoc> ssl(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    CBl2Seq blaster(*qsl, *ssl, eBlastp);

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;

    const CBlastOptions& kOpts = blaster.GetOptionsHandle().GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(const_cast<TSeqLocVector&>(blaster.GetQueries()), 
                   prog, strand_opt, &query_info);
    SetupQueries(const_cast<TSeqLocVector&>(blaster.GetQueries()), 
                 query_info, &query_blk, prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_CHECK(m->empty());
    }

    BlastScoringOptions* scoring_options;
    BlastScoringOptionsNew(kBlastProgram, &scoring_options);
    BLAST_FillScoringOptions(scoring_options, kBlastProgram, FALSE, 0, 0,
          "BLOSUM62", -1, -1);
    scoring_options->gapped_calculation = FALSE;
   
    BlastScoreBlk* sbp;
    Blast_Message* blast_message = NULL;
    BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_options, 
                             kBlastProgram, &sbp, 1.0, &blast_message,
                            &BlastFindMatrixPath);

    s_FillSearchSpace(query_info, 98483910471LL);

    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options,
                             scoring_options->gapped_calculation);

    BlastHitSavingOptions* hit_options;
    BlastHitSavingOptionsNew(kBlastProgram, &hit_options,
                             scoring_options->gapped_calculation);
    BOOST_CHECK(hit_options->do_sum_stats);

    BlastHitSavingParameters* hit_params;
    BlastHitSavingParametersNew(kBlastProgram, hit_options, sbp, query_info, kAvgSubjectLength, 2, &hit_params);
    BlastLinkHSPParameters* link_hsp_params = hit_params->link_hsp_params;

    BlastInitialWordOptions* word_options;
    BlastInitialWordOptionsNew(kBlastProgram, &word_options);

    BlastInitialWordParameters* word_params = s_GetInitialWordParameters(kBlastProgram, query_blk,
          query_info, sbp, word_options, kAvgSubjectLength, hit_params);


    /* Reset gap_prob to zero to see that it's properly put back to correct values. */
    link_hsp_params->gap_prob = 0.0; 
    link_hsp_params->gap_decay_rate = 0.5;

    CalculateLinkHSPCutoffs(kBlastProgram, query_info, sbp, 
                            link_hsp_params, word_params, 
                            kDbLength, kSpecificSubjectLength);

    BOOST_CHECK_EQUAL(5, (int) (10*link_hsp_params->gap_prob));
    BOOST_CHECK_EQUAL(36, link_hsp_params->cutoff_big_gap);
    BOOST_CHECK_EQUAL(41, link_hsp_params->cutoff_small_gap); 

    query_blk = BlastSequenceBlkFree(query_blk);
    sbp = BlastScoreBlkFree(sbp);
    scoring_options = BlastScoringOptionsFree(scoring_options);
    ext_options = BlastExtensionOptionsFree(ext_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    word_params = BlastInitialWordParametersFree(word_params);
    word_options = BlastInitialWordOptionsFree(word_options);
    return;
}

BOOST_AUTO_TEST_CASE( testLargeWordSize )
{
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    const Boolean k_is_megablast = FALSE;
    const int k_threshold = 0;
    const int k_word_size = 100000;  /* Word-size bigger than INT2_MAX. */

    CBlastInitialWordOptions word_options;
    BlastInitialWordOptionsNew(kProgram, &word_options);
    CLookupTableOptions lookup_options;
    LookupTableOptionsNew(kProgram, &lookup_options);
    BLAST_FillLookupTableOptions(lookup_options, kProgram,
                                 k_is_megablast, k_threshold, k_word_size);

    BOOST_CHECK_EQUAL(k_word_size, (int) lookup_options->word_size);
}

static void MakeSomeInvalidKBP(Blast_KarlinBlk** kbp_array, 
                               Int4 num, 
                               Int4 good_one,
                               BlastQueryInfo* query_info)
{
     Int4 index;
 
     BOOST_REQUIRE(num > good_one);
     
     for (index=0; index<num; index++)
     {
        Blast_KarlinBlk* kbp = NULL;
        Blast_KarlinBlkFree(kbp_array[index]);
        if (index != good_one)
        {
            kbp = Blast_KarlinBlkNew();
            kbp->Lambda = -1;
            kbp->K = -1;
            kbp->H = -1;
            query_info->contexts[index].is_valid = FALSE;
        }
        else
        {
            kbp = Blast_KarlinBlkNew();
            kbp->Lambda = 1.37;
            kbp->K = 0.71;
            kbp->logK = -0.34;
            kbp->H = 1.3;
            query_info->contexts[index].is_valid = TRUE;
        }
        kbp_array[index] = kbp;
     }
     return;
}


BOOST_AUTO_TEST_CASE( testExtParamNewSomeInvalidKbp )
{
    const int k_num_contexts=6;
    const EBlastProgramType kBlastProgram=eBlastTypeBlastn;
    
    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options, true);
    ext_options->gap_x_dropoff = 20;
    ext_options->gap_x_dropoff_final = 20;
/*  FIXME
    ext_options->gap_trigger = 20;
*/

    CBlastQueryInfo query_info(BlastQueryInfoNew(eBlastTypeBlastx, 1));
    BlastScoreBlk sb;
    sb.kbp = (Blast_KarlinBlk**) calloc(k_num_contexts, sizeof(Blast_KarlinBlk*)); 
    sb.kbp_gap = (Blast_KarlinBlk**) calloc(k_num_contexts, sizeof(Blast_KarlinBlk*)); 
    MakeSomeInvalidKBP(sb.kbp, k_num_contexts, 4, query_info);
    MakeSomeInvalidKBP(sb.kbp_gap, k_num_contexts, 4, query_info);
    sb.scale_factor = 0.0;
    sb.matrix_only_scoring = false;

    BlastExtensionParameters* ext_params;
    BlastExtensionParametersNew(kBlastProgram, ext_options, &sb,
                                query_info, &ext_params);
 
    
    BOOST_CHECK(ext_params->gap_x_dropoff > 0.0);
    BOOST_CHECK(ext_params->gap_x_dropoff_final > 0.0);
/*  FIXME
    BOOST_CHECK(ext_params->gap_trigger > 0.0);
*/

    for (int index=query_info->first_context; index<=query_info->last_context; index++)
    {
       sb.kbp[index] = Blast_KarlinBlkFree(sb.kbp[index]);
       sb.kbp_gap[index] = Blast_KarlinBlkFree(sb.kbp_gap[index]);
    }
    sfree(sb.kbp);
    sfree(sb.kbp_gap);
    ext_params = BlastExtensionParametersFree(ext_params);
    ext_options = BlastExtensionOptionsFree(ext_options);
}

static void MakeSomeValidKBP(Blast_KarlinBlk** kbp_array, Int4 num,
                             BlastQueryInfo* query_info)
{
     for (Int4 index=0; index<num; index++)
     {
        Blast_KarlinBlk* kbp = NULL;
        Blast_KarlinBlkFree(kbp_array[index]);
        kbp = Blast_KarlinBlkNew();
        kbp->Lambda = 1.30;
        kbp->K = 0.71;
        kbp->logK = -0.34;
        kbp->H = 1.3;
        kbp_array[index] = kbp;
        query_info->contexts[index].is_valid = TRUE;
     }
     return;
}

BOOST_AUTO_TEST_CASE( testExtensionParamsNew )
{
    const int k_num_contexts=6;
    const EBlastProgramType kBlastProgram=eBlastTypeBlastn;

    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options, true);
    ext_options->gap_x_dropoff = 20;
    ext_options->gap_x_dropoff_final = 22;

    CBlastQueryInfo query_info(BlastQueryInfoNew(eBlastTypeBlastx, 1));
    BlastScoreBlk sb;
    sb.kbp = (Blast_KarlinBlk**) calloc(k_num_contexts, sizeof(Blast_KarlinBlk*));
    sb.kbp_gap = (Blast_KarlinBlk**) calloc(k_num_contexts, sizeof(Blast_KarlinBlk*));
    MakeSomeValidKBP(sb.kbp, k_num_contexts, query_info.Get());
    MakeSomeValidKBP(sb.kbp_gap, k_num_contexts, query_info.Get());
    sb.scale_factor = 0.0;
    sb.matrix_only_scoring = FALSE;


    BlastExtensionParameters* ext_params;
    BlastExtensionParametersNew(kBlastProgram, ext_options, &sb,
                                query_info, &ext_params);


    BOOST_CHECK_EQUAL(10, ext_params->gap_x_dropoff);
    BOOST_CHECK_EQUAL(11, ext_params->gap_x_dropoff_final);

    ext_params = BlastExtensionParametersFree(ext_params);

    // gap_x_dropoff_final less than gap_x_dropoff, gap_x_dropoff_final should be adjusted.
    ext_options->gap_x_dropoff = 25;
    ext_options->gap_x_dropoff_final = 22;
    BlastExtensionParametersNew(kBlastProgram, ext_options, &sb,
                                query_info, &ext_params);


    BOOST_CHECK_EQUAL(13, ext_params->gap_x_dropoff);
    BOOST_CHECK_EQUAL(13, ext_params->gap_x_dropoff_final);

    for (int index=query_info->first_context; index<=query_info->last_context; index++)
    {
       sb.kbp[index] = Blast_KarlinBlkFree(sb.kbp[index]);
       sb.kbp_gap[index] = Blast_KarlinBlkFree(sb.kbp_gap[index]);
    }
    sfree(sb.kbp);
    sfree(sb.kbp_gap);
    ext_params = BlastExtensionParametersFree(ext_params);
    ext_options = BlastExtensionOptionsFree(ext_options);
   }

BOOST_AUTO_TEST_CASE( testHitSavingParamNewSomeInvalidKbp )
{
    const EBlastProgramType kBlastProgram = eBlastTypeBlastn;
    CSeq_id qid1("gi|555");
    auto_ptr<SSeqLoc> qsl1(
        CTestObjMgr::Instance().CreateSSeqLoc(qid1, eNa_strand_both));
    CSeq_id qid2("gi|556");
    auto_ptr<SSeqLoc> qsl2(
        CTestObjMgr::Instance().CreateSSeqLoc(qid2, eNa_strand_both));
    CSeq_id qid3("gi|557");
    auto_ptr<SSeqLoc> qsl3(
        CTestObjMgr::Instance().CreateSSeqLoc(qid3, eNa_strand_both));

    TSeqLocVector query_v;

    query_v.push_back(*qsl1);
    query_v.push_back(*qsl2);
    query_v.push_back(*qsl3);

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
        BOOST_CHECK(m->empty());
    }

    BlastScoringOptions* scoring_options;
    BlastScoringOptionsNew(kBlastProgram, &scoring_options);
    BLAST_FillScoringOptions(scoring_options, kBlastProgram, FALSE, 0, 0,
          "BLOSUM62", -1, -1);

    BlastScoreBlk* sbp;
    Blast_Message* blast_message = NULL;
    BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_options,
                             kBlastProgram, &sbp, 1.0, &blast_message,
                            &BlastFindMatrixPath);

    // Here we remove the valid KarlinBlks and put in some that might result from completely masked queries.
    MakeSomeInvalidKBP(sbp->kbp, 
                       query_info->last_context+1, 
                       query_info->last_context-1,
                       query_info.Get());
    MakeSomeInvalidKBP(sbp->kbp_gap, 
                       query_info->last_context+1, 
                       query_info->last_context-1,
                       query_info.Get());

    s_FillSearchSpace(query_info, 10000000LL);

    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options,
                             scoring_options->gapped_calculation);

    BlastHitSavingOptions* hit_options;
    BlastHitSavingOptionsNew(kBlastProgram, &hit_options,
                             scoring_options->gapped_calculation);

    BlastHitSavingParameters* hit_params;
    BlastHitSavingParametersNew(kBlastProgram, hit_options, sbp, query_info, 0, 0, &hit_params);

    BOOST_CHECK_EQUAL(10, hit_params->cutoff_score_min);

    scoring_options = BlastScoringOptionsFree(scoring_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    ext_options = BlastExtensionOptionsFree(ext_options);
    sbp = BlastScoreBlkFree(sbp);
}

// This simulates values calculated by for the human genomic database.
// the cutoff_score is larger than the score corresponding to an expect 
// value of 10, so it gets set to that value.
BOOST_AUTO_TEST_CASE( testHitSavingParamNewGappedTblastnLargeSubjectSequence )
{
    const EBlastProgramType kBlastProgram = eBlastTypeTblastn;
    CSeq_id qid1("gi|3091");
    auto_ptr<SSeqLoc> qsl1(
        CTestObjMgr::Instance().CreateSSeqLoc(qid1, eNa_strand_both));

    TSeqLocVector query_v;
    query_v.push_back(*qsl1);

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eTblastn));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info);
    SetupQueries(query_v, query_info, &query_blk, 
                 prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_CHECK(m->empty());
    }


    BlastScoringOptions* scoring_options;
    BlastScoringOptionsNew(kBlastProgram, &scoring_options);
    BLAST_FillScoringOptions(scoring_options, kBlastProgram, FALSE, 0, 0,
          "BLOSUM62", -1, -1);

    BlastScoreBlk* sbp;
    Blast_Message* blast_message = NULL;
    BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_options,
                             kBlastProgram, &sbp, 1.0, &blast_message,
                            &BlastFindMatrixPath);

    s_FillSearchSpace(query_info, 481002014850LL);

    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options,
                             scoring_options->gapped_calculation);

    BlastHitSavingOptions* hit_options;
    BlastHitSavingOptionsNew(kBlastProgram, &hit_options,
                             scoring_options->gapped_calculation);

    const int k_avg_subject_length=128199245;
    BlastHitSavingParameters* hit_params;
    BlastHitSavingParametersNew(kBlastProgram, hit_options, sbp, query_info, k_avg_subject_length, 2, &hit_params);

    BOOST_CHECK_EQUAL(66, hit_params->cutoff_score_min);
    BOOST_CHECK_EQUAL(50, hit_params->prelim_evalue);

    scoring_options = BlastScoringOptionsFree(scoring_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    ext_options = BlastExtensionOptionsFree(ext_options);
    sbp = BlastScoreBlkFree(sbp);
}


// This simulates values calculated by for the drosoph database.
// the cutoff_score is between gap_trigger and the score corresponding 
// to an expect value of 10.
BOOST_AUTO_TEST_CASE( testHitSavingParamNewGappedTblastnMidsizeSubjectSequence )
{
    const EBlastProgramType kBlastProgram = eBlastTypeTblastn;
    CSeq_id qid1("gi|3091");
    auto_ptr<SSeqLoc> qsl1(
        CTestObjMgr::Instance().CreateSSeqLoc(qid1, eNa_strand_both));

    TSeqLocVector query_v;
    query_v.push_back(*qsl1);

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eTblastn));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info);
    SetupQueries(query_v, query_info, &query_blk, 
                 prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_CHECK(m->empty());
    }

    BlastScoringOptions* scoring_options;
    BlastScoringOptionsNew(kBlastProgram, &scoring_options);
    BLAST_FillScoringOptions(scoring_options, kBlastProgram, FALSE, 0, 0,
          "BLOSUM62", -1, -1);

    BlastScoreBlk* sbp;
    Blast_Message* blast_message = NULL;
    BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_options,
                             kBlastProgram, &sbp, 1.0, &blast_message,
                            &BlastFindMatrixPath);

    s_FillSearchSpace(query_info, 20007999590LL);

    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options,
                             scoring_options->gapped_calculation);

    BlastHitSavingOptions* hit_options;
    BlastHitSavingOptionsNew(kBlastProgram, &hit_options,
                             scoring_options->gapped_calculation);

    const int k_avg_subject_length=104833;
    BlastHitSavingParameters* hit_params;
    BlastHitSavingParametersNew(kBlastProgram, hit_options, sbp, query_info, k_avg_subject_length, 2, &hit_params);

    BOOST_CHECK_EQUAL(40, hit_params->cutoff_score_min);
    BOOST_CHECK_EQUAL(50, hit_params->prelim_evalue);

    scoring_options = BlastScoringOptionsFree(scoring_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    ext_options = BlastExtensionOptionsFree(ext_options);
    sbp = BlastScoreBlkFree(sbp);
}

// This checks that for repeated calls to BlastHitSavingParametersUpdate the
// proper value for cutoff_score is returned.
BOOST_AUTO_TEST_CASE( testHitSavingParamUpdateMultipleCalls )
{
    const EBlastProgramType kBlastProgram = eBlastTypeTblastn;
    CSeq_id qid1("gi|3091");
    auto_ptr<SSeqLoc> qsl1(
        CTestObjMgr::Instance().CreateSSeqLoc(qid1, eNa_strand_both));

    TSeqLocVector query_v;
    query_v.push_back(*qsl1);

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eTblastn));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info);
    SetupQueries(query_v, query_info, &query_blk, 
                 prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_CHECK(m->empty());
    }

    BlastScoringOptions* scoring_options;
    BlastScoringOptionsNew(kBlastProgram, &scoring_options);
    BLAST_FillScoringOptions(scoring_options, kBlastProgram, FALSE, 0, 0,
          "BLOSUM62", -1, -1);

    BlastScoreBlk* sbp;
    Blast_Message* blast_message = NULL;
    BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_options,
                             kBlastProgram, &sbp, 1.0, &blast_message,
                            &BlastFindMatrixPath);

    s_FillSearchSpace(query_info, 20007999590LL);

    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options,
                             scoring_options->gapped_calculation);

    BlastHitSavingOptions* hit_options;
    BlastHitSavingOptionsNew(kBlastProgram, &hit_options,
                             scoring_options->gapped_calculation);

    const int k_avg_subject_length=104833;
    BlastHitSavingParameters* hit_params;
    BlastHitSavingParametersNew(kBlastProgram, hit_options, sbp, query_info, k_avg_subject_length, 2, &hit_params);

    BOOST_CHECK_EQUAL(40, hit_params->cutoff_score_min);
    BOOST_CHECK_EQUAL(40, hit_params->cutoffs[0].cutoff_score);
    BOOST_CHECK_EQUAL(40, hit_params->cutoffs[0].cutoff_score_max);

    s_FillSearchSpace(query_info, 2000799959LL);

    BlastHitSavingParametersUpdate(kBlastProgram, sbp, query_info, k_avg_subject_length, 2, hit_params);
    BOOST_CHECK_EQUAL(50, hit_params->prelim_evalue);
    BOOST_CHECK_EQUAL(40, hit_params->cutoff_score_min);
    BOOST_CHECK_EQUAL(40, hit_params->cutoffs[0].cutoff_score);
    BOOST_CHECK_EQUAL(40, hit_params->cutoffs[0].cutoff_score_max);

    scoring_options = BlastScoringOptionsFree(scoring_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    ext_options = BlastExtensionOptionsFree(ext_options);
    sbp = BlastScoreBlkFree(sbp);
}

// This simulates values calculated by for an EST database.
// the cutoff_score is actually less than gap_trigger and gets
// raised to that level.
BOOST_AUTO_TEST_CASE( testHitSavingParamNewGappedTblastnSmallSubjectSequence )
{
    const EBlastProgramType kBlastProgram = eBlastTypeTblastn;
    CSeq_id qid1("gi|17532675");
    auto_ptr<SSeqLoc> qsl1(
        CTestObjMgr::Instance().CreateSSeqLoc(qid1, eNa_strand_both));

    TSeqLocVector query_v;
    query_v.push_back(*qsl1);

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eTblastn));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info);
    SetupQueries(query_v, query_info, &query_blk, 
                 prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_CHECK(m->empty());
    }

    BlastScoringOptions* scoring_options;
    BlastScoringOptionsNew(kBlastProgram, &scoring_options);
    BLAST_FillScoringOptions(scoring_options, kBlastProgram, FALSE, 0, 0,
          "BLOSUM62", -1, -1);

    BlastScoreBlk* sbp;
    Blast_Message* blast_message = NULL;
    BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_options,
                             kBlastProgram, &sbp, 1.0, &blast_message,
                            &BlastFindMatrixPath);

    s_FillSearchSpace(query_info, 1480902925051LL);

    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options,
                             scoring_options->gapped_calculation);

    BlastHitSavingOptions* hit_options;
    BlastHitSavingOptionsNew(kBlastProgram, &hit_options,
                             scoring_options->gapped_calculation);

    const int k_avg_subject_length=523;
    BlastHitSavingParameters* hit_params;
    BlastHitSavingParametersNew(kBlastProgram, hit_options, sbp, query_info, k_avg_subject_length, 2, &hit_params);

    BOOST_CHECK_EQUAL(19, hit_params->cutoff_score_min);
    BOOST_CHECK_EQUAL(19, hit_params->cutoffs[0].cutoff_score);
    BOOST_CHECK_EQUAL(19, hit_params->cutoffs[0].cutoff_score_max);

    scoring_options = BlastScoringOptionsFree(scoring_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    ext_options = BlastExtensionOptionsFree(ext_options);
    sbp = BlastScoreBlkFree(sbp);
}


BOOST_AUTO_TEST_CASE( testInitialWordParamNewSomeInvalidKbp )
{
    const EBlastProgramType kBlastProgram = eBlastTypeBlastn;
    const Uint4 k_subject_length=10000;
    CSeq_id qid1("gi|555");
    auto_ptr<SSeqLoc> qsl1(
        CTestObjMgr::Instance().CreateSSeqLoc(qid1, eNa_strand_both));
    CSeq_id qid2("gi|556");
    auto_ptr<SSeqLoc> qsl2(
        CTestObjMgr::Instance().CreateSSeqLoc(qid2, eNa_strand_both));
    CSeq_id qid3("gi|557");
    auto_ptr<SSeqLoc> qsl3(
        CTestObjMgr::Instance().CreateSSeqLoc(qid3, eNa_strand_both));

    TSeqLocVector query_v;

    query_v.push_back(*qsl1);
    query_v.push_back(*qsl2);
    query_v.push_back(*qsl3);

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
        BOOST_CHECK(m->empty());
    }

    BlastScoringOptions* scoring_options;
    BlastScoringOptionsNew(kBlastProgram, &scoring_options);
    BLAST_FillScoringOptions(scoring_options, kBlastProgram, FALSE, 0, 0,
          "BLOSUM62", -1, -1);

    BlastScoreBlk* sbp;
    Blast_Message* blast_message = NULL;
    BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_options,
                             kBlastProgram, &sbp, 1.0, &blast_message,
                            &BlastFindMatrixPath);

    // Here we remove the valid KarlinBlks and put in some that might result from completely masked queries.
    MakeSomeInvalidKBP(sbp->kbp, query_info->last_context+1,
                       query_info->last_context-1, query_info.Get());
    MakeSomeInvalidKBP(sbp->kbp_gap, query_info->last_context+1,
                       query_info->last_context-1, query_info.Get());

    s_FillSearchSpace(query_info, 1480902925051LL);

    BlastExtensionOptions* ext_options;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options,
                             scoring_options->gapped_calculation);

    BlastHitSavingOptions* hit_options;
    BlastHitSavingOptionsNew(kBlastProgram, &hit_options,
                             scoring_options->gapped_calculation);
    BlastHitSavingParameters* hit_params;
    BlastHitSavingParametersNew(kBlastProgram, hit_options, sbp, query_info, 0, 0, &hit_params);
    hit_params->cutoff_score_min = 19;

    BlastInitialWordOptions* word_options;
    BlastInitialWordOptionsNew(kBlastProgram, &word_options);
    word_options->x_dropoff = 20;

    BlastInitialWordParameters* word_params = s_GetInitialWordParameters(kBlastProgram, query_blk,
          query_info, sbp, word_options, k_subject_length, hit_params);

    BOOST_CHECK_EQUAL(13, word_params->cutoff_score_min);
    BOOST_CHECK_EQUAL(11, word_params->x_dropoff_max);

    scoring_options = BlastScoringOptionsFree(scoring_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    word_params = BlastInitialWordParametersFree(word_params);
    word_options = BlastInitialWordOptionsFree(word_options);
    ext_options = BlastExtensionOptionsFree(ext_options);
    sbp = BlastScoreBlkFree(sbp);
}

BOOST_AUTO_TEST_CASE( testRemoteFilterString)
{
       typedef ncbi::objects::CBlast4_parameters TBlast4Opts;
       CBlastOptions opts(CBlastOptions::eRemote);

       opts.SetProgram(eBlastn);
       opts.SetFilterString("F", true);/* NCBI_FAKE_WARNING */
       // cerr << "dust filter" << (int) blast4_opts->GetParamByName("DustFiltering")->GetValue().GetBoolean() << '\n';
       BOOST_CHECK_EQUAL(false, opts.GetBlast4AlgoOpts()->GetParamByName("DustFiltering")->GetValue().GetBoolean());
       BOOST_CHECK_EQUAL(false, opts.GetBlast4AlgoOpts()->GetParamByName("RepeatFiltering")->GetValue().GetBoolean());
       opts.SetFilterString("T", true);/* NCBI_FAKE_WARNING */
       BOOST_CHECK_EQUAL(true, opts.GetBlast4AlgoOpts()->GetParamByName("DustFiltering")->GetValue().GetBoolean());
       BOOST_CHECK_EQUAL(false, opts.GetBlast4AlgoOpts()->GetParamByName("RepeatFiltering")->GetValue().GetBoolean());

       opts.SetProgram(eBlastp);
       opts.SetFilterString("F", true);/* NCBI_FAKE_WARNING */
       BOOST_CHECK_EQUAL(false, opts.GetBlast4AlgoOpts()->GetParamByName("SegFiltering")->GetValue().GetBoolean());
       opts.SetFilterString("T", true);/* NCBI_FAKE_WARNING */
       BOOST_CHECK_EQUAL(true, opts.GetBlast4AlgoOpts()->GetParamByName("SegFiltering")->GetValue().GetBoolean());

       opts.SetProgram(eBlastx);
       opts.SetFilterString("F", true);/* NCBI_FAKE_WARNING */
       BOOST_CHECK_EQUAL(false, opts.GetBlast4AlgoOpts()->GetParamByName("SegFiltering")->GetValue().GetBoolean());
       opts.SetFilterString("T", true);/* NCBI_FAKE_WARNING */
       BOOST_CHECK_EQUAL(true, opts.GetBlast4AlgoOpts()->GetParamByName("SegFiltering")->GetValue().GetBoolean());

       opts.SetProgram(eTblastn);
       opts.SetFilterString("F", true);/* NCBI_FAKE_WARNING */
       BOOST_CHECK_EQUAL(false, opts.GetBlast4AlgoOpts()->GetParamByName("SegFiltering")->GetValue().GetBoolean());
       opts.SetFilterString("T", true);/* NCBI_FAKE_WARNING */
       BOOST_CHECK_EQUAL(true, opts.GetBlast4AlgoOpts()->GetParamByName("SegFiltering")->GetValue().GetBoolean());

       opts.SetProgram(eTblastx);
       opts.SetFilterString("F", true);/* NCBI_FAKE_WARNING */
       BOOST_CHECK_EQUAL(false, opts.GetBlast4AlgoOpts()->GetParamByName("SegFiltering")->GetValue().GetBoolean());
       opts.SetFilterString("T", true);/* NCBI_FAKE_WARNING */
       BOOST_CHECK_EQUAL(true, opts.GetBlast4AlgoOpts()->GetParamByName("SegFiltering")->GetValue().GetBoolean());
}

BOOST_AUTO_TEST_CASE( testNewFilteringDefaults )
{
    CRef<CBlastOptionsHandle> opts;
    
    opts = CBlastOptionsFactory::Create(eTblastn);
    BOOST_REQUIRE(opts.NotEmpty());
    char* filter_string = opts->GetFilterString(); /* NCBI_FAKE_WARNING */
    BOOST_CHECK_EQUAL(string("F"), string(filter_string));
    sfree(filter_string);

    opts = CBlastOptionsFactory::Create(eBlastp);
    BOOST_REQUIRE(opts.NotEmpty());
    filter_string = opts->GetFilterString(); /* NCBI_FAKE_WARNING */
    BOOST_CHECK_EQUAL(string("F"), string(filter_string));
    sfree(filter_string);
}

BOOST_AUTO_TEST_CASE( testOptionsDeepCopy )
{
    CRef<CBlastOptionsHandle> optsHandle;
    
    optsHandle = CBlastOptionsFactory::Create(eBlastp);
    BOOST_REQUIRE(optsHandle.NotEmpty());

    optsHandle->SetFilterString("L;m;"); /* NCBI_FAKE_WARNING */
    optsHandle->SetDbLength(10000);
    optsHandle->SetOptions().SetPHIPattern("Y-S-[SA]-X-[LVIM]", false);
    //optsHandle->GetOptions().DebugDumpText(NcbiCerr, "BLAST options - original", 1);

    CRef<CBlastOptions> optsClone = optsHandle->GetOptions().Clone();
    optsHandle.Reset();
    //optsClone->DebugDumpText(NcbiCerr, "BLAST options - clone", 1);

    BOOST_CHECK_EQUAL(optsClone->GetDbLength(), 10000);
    BOOST_CHECK_EQUAL(string(optsClone->GetFilterString()), string("L;m;")); /* NCBI_FAKE_WARNING */
    BOOST_CHECK_EQUAL(string(optsClone->GetPHIPattern()), string("Y-S-[SA]-X-[LVIM]"));

    // try setting and unsetting the best hit options (SB-339, issue #4)
    optsClone->SetBestHitScoreEdge(kBestHit_ScoreEdgeDflt);
    optsClone->SetBestHitOverhang(kBestHit_OverhangDflt);
    CRef<CBlastOptions> optsSnapshot = optsClone->Clone();

    optsClone->SetBestHitScoreEdge(kBestHit_ScoreEdgeDflt * 2);
    optsClone->SetBestHitOverhang(kBestHit_OverhangDflt * 2);
    BOOST_CHECK_CLOSE(optsClone->GetBestHitScoreEdge(), kBestHit_ScoreEdgeDflt * 2, 0.00000001);
    BOOST_CHECK_CLOSE(optsClone->GetBestHitOverhang(), kBestHit_OverhangDflt * 2, 0.00000001);

    optsClone = optsSnapshot;
    BOOST_CHECK_CLOSE(optsClone->GetBestHitScoreEdge(), kBestHit_ScoreEdgeDflt, 0.00000001);
    BOOST_CHECK_CLOSE(optsClone->GetBestHitOverhang(), kBestHit_OverhangDflt, 0.00000001);
}

BOOST_AUTO_TEST_SUITE_END()
#endif /* SKIP_DOXYGEN_PROCESSING */
