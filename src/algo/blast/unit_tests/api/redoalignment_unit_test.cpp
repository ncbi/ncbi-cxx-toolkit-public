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
*   Unit test module for traceback calculation
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <corelib/ncbitime.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/objostr.hpp>

#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/seqsrc_multiseq.hpp>
#include <blast_objmgr_priv.hpp>
#include <blast_psi_priv.h>

#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_kappa.h>
#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/hspfilter_collector.h>

#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_prot_options.hpp>

#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>

#include "test_objmgr.hpp"
#include "blast_test_util.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

struct CRedoAlignmentTestFixture
{
    CRedoAlignmentTestFixture() {}
    ~CRedoAlignmentTestFixture() {}

    static BlastHSPList* setUpHSPList(int num_hsps,
                                      const int query_offset[],
                                      const int query_end[],
                                      const int subject_offset[],
                                      const int subject_end[],
                                      const int query_gapped_start[],
                                      const int subject_gapped_start[],
                                      const int score[],
                                      const double evalue[] = NULL,
                                      const int num_ident[] = NULL)
    {
        const int kQueryContext = 0;
        const int kSubjectFrame = 0;
        const int kQueryFrame = 0;

        BlastHSPList* retval = Blast_HSPListNew(0);
        if ( !retval ) {
            return NULL;
        }

        for (int i = 0; i < num_hsps; i++) {
            BlastHSP* hsp = NULL;
            Blast_HSPInit(query_offset[i], query_end[i],
                          subject_offset[i], subject_end[i],
                          query_gapped_start[i],
                          subject_gapped_start[i],
                          kQueryContext, kQueryFrame, kSubjectFrame, score[i],
                          NULL, &hsp);
            if (num_ident) {
                hsp->num_ident = num_ident[i];
            }
            if (evalue) {
                hsp->evalue = evalue[i];
            }
            Blast_HSPListSaveHSP(retval, hsp);
        }

        return retval;
    }

    static int** loadPssmFromFile(const string& filename, 
                                  unsigned int query_length)
    {
        ifstream in(filename.c_str());
        if ( !in ) {
            throw runtime_error(filename + " could not be found");
        }

        int** retval = (int**) _PSIAllocateMatrix(query_length,
                                                  BLASTAA_SIZE,
                                                  sizeof(int));
        try {
            for (unsigned int i = 0; i < query_length; i++) {
                for (unsigned int j = 0; j < BLASTAA_SIZE; j++) {
                    in >> retval[i][j];
                }
            }
        } catch (...) {
            retval = (int**) _PSIDeallocateMatrix((void**)retval, query_length);
            throw runtime_error("Error reading from " + filename);
        }

        return retval;
    }

    static void setupPositionBasedBlastScoreBlk(BlastScoreBlk* sbp,
                                                unsigned int qlen)
    {
        if ( !sbp ) {
            throw runtime_error("NULL BlastScoreBlk*!");
        }

        int** pssm =
            loadPssmFromFile("data/aa.129295.pssm.txt",
                                                      qlen);
        sbp->psi_matrix = SPsiBlastScoreMatrixNew(qlen);
        _PSICopyMatrix_int(sbp->psi_matrix->pssm->data, pssm,
                           qlen, BLASTAA_SIZE);
        pssm = (int**)_PSIDeallocateMatrix((void**) pssm, qlen);

        /* FIXME: Should offer a function that allows the setting of all
         * PSI-BLAST settings in the BlastScoreBlk */
        sbp->kbp = sbp->kbp_psi;
        sbp->kbp_gap = sbp->kbp_gap_psi;
    }

    // Core function which executes the unit tests
    static void runRedoAlignmentCoreUnitTest(EBlastProgramType program,
                                    CSeq_id& qid,
                                    CSeq_id& sid,
                                    BlastHSPList* init_hsp_list,
                                    const BlastHSPList* ending_hsp_list,
                                    Int8 effective_searchsp,
                                    ECompoAdjustModes compositonBasedStatsMode,
                                    bool doSmithWaterman,
                                    double evalue_threshold =
                                    BLAST_EXPECT_VALUE,
                                    int hit_list_size = BLAST_HITLIST_SIZE);

    // Core function which executes the unit tests
    // FIXME: refactor so that blastOptions are passed in!
    static void runRedoAlignmentCoreUnitTest(EBlastProgramType program,
                                    SSeqLoc& qsl,
                                    SSeqLoc& ssl,
                                    BlastHSPList* init_hsp_list,
                                    const BlastHSPList* ending_hsp_list,
                                    Int8 effective_searchsp,
                                    ECompoAdjustModes compositonBasedStatsMode,
                                    bool doSmithWaterman,
                                    double evalue_threshold =
                                    BLAST_EXPECT_VALUE,
                                    int hit_list_size = BLAST_HITLIST_SIZE);
};

void CRedoAlignmentTestFixture::
        runRedoAlignmentCoreUnitTest(EBlastProgramType program,
                                    CSeq_id& qid,
                                    CSeq_id& sid,
                                    BlastHSPList* init_hsp_list,
                                    const BlastHSPList* ending_hsp_list,
                                    Int8 effective_searchsp,
                                    ECompoAdjustModes compositonBasedStatsMode,
                                    bool doSmithWaterman,
                                    double evalue_threshold,
                                    int hit_list_size)
{
    
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(qid));
    auto_ptr<SSeqLoc> ssl(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    runRedoAlignmentCoreUnitTest(program, *qsl, *ssl, init_hsp_list,
            ending_hsp_list, effective_searchsp, compositonBasedStatsMode,
            doSmithWaterman, evalue_threshold, hit_list_size);

}


void CRedoAlignmentTestFixture::
        runRedoAlignmentCoreUnitTest(EBlastProgramType program,
                                    SSeqLoc& qsl,
                                    SSeqLoc& ssl,
                                    BlastHSPList* init_hsp_list,
                                    const BlastHSPList* ending_hsp_list,
                                    Int8 effective_searchsp,
                                    ECompoAdjustModes compositonBasedStatsMode,
                                    bool doSmithWaterman,
                                    double evalue_threshold,
                                    int hit_list_size)
{

    char* program_buffer = NULL;
    Int2 rv = BlastNumber2Program(program, &program_buffer);
    BOOST_REQUIRE_MESSAGE(rv == (Int2)0, "BlastNumber2Program failed");
    blast::EProgram prog = blast::ProgramNameToEnum(string(program_buffer));
    sfree(program_buffer);
    CBl2Seq blaster(qsl, ssl, prog);

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;

    const CBlastOptions& kOpts = blaster.GetOptionsHandle().GetOptions();
    EBlastProgramType core_prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(const_cast<TSeqLocVector&>(blaster.GetQueries()), 
                    core_prog, strand_opt,&query_info);
    SetupQueries(const_cast<TSeqLocVector&>(blaster.GetQueries()), 
                    query_info, &query_blk, core_prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_REQUIRE(m->empty());
    }

    CBlastScoringOptions scoring_opts;
    rv = BlastScoringOptionsNew(program, &scoring_opts);
    BOOST_REQUIRE(rv == 0);

    CBlastExtensionOptions ext_opts;
    BlastExtensionOptionsNew(program, &ext_opts,
                                scoring_opts->gapped_calculation);

    CBlastHitSavingOptions hitsaving_opts;
    rv = BlastHitSavingOptionsNew(program, &hitsaving_opts,
                                    scoring_opts->gapped_calculation);
    hitsaving_opts->expect_value = evalue_threshold;
    hitsaving_opts->hitlist_size = hit_list_size;
    BOOST_REQUIRE(rv == 0);

    // FIXME: how to deal with this in case of PSI-BLAST?
    // FIXME: GetQueryEncoding/GetSubjectEncoding for PSI-BLAST
    // FIXME: Figure out what is needed to set up a PSI-BLAST search. Is
    // lookup table different than eAaLookupTable? See
    // PsiBlastOptionsHandle class also
    CBlastSeqSrc seq_src
        (MultiSeqBlastSeqSrcInit(
                    const_cast<TSeqLocVector&>(blaster.GetSubjects()), 
                    blaster.GetOptionsHandle().GetOptions().GetProgramType()));
    TestUtil::CheckForBlastSeqSrcErrors(seq_src);

    BlastExtensionOptions* ext_options=NULL;
    BlastExtensionOptionsNew(program, &ext_options, true);
    ext_options->compositionBasedStats = compositonBasedStatsMode;
    if (doSmithWaterman)
        ext_options->eTbackExt = eSmithWatermanTbck;

    BlastHSPWriterInfo * writer_info = BlastHSPCollectorInfoNew(
	        BlastHSPCollectorParamsNew(
		hitsaving_opts, ext_options->compositionBasedStats,
        scoring_opts->gapped_calculation));

    BlastHSPWriter* writer = BlastHSPWriterNew(&writer_info, NULL);
    BOOST_REQUIRE(writer_info == NULL);

    BlastHSPStream* hsp_stream = BlastHSPStreamNew(
		program, ext_options, FALSE, query_info->num_queries, writer);

    Blast_HSPListSortByScore(init_hsp_list);
    BlastHSPStreamWrite(hsp_stream, &init_hsp_list);
        
    Blast_Message* blast_message=NULL;
    BlastScoreBlk* sbp;
    Int2 status;
    /* FIXME: modularize this code */
    const double k_rps_scale_factor = 1.0;   
    status = 
        BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_opts,
                                program, &sbp, k_rps_scale_factor, 
                                &blast_message, &BlastFindMatrixPath);
    if (status > 0) {
        return;
    }

    if (program == eBlastTypePsiBlast) {
        setupPositionBasedBlastScoreBlk(sbp,
                sequence::GetLength(*(qsl.seqloc->GetId()), qsl.scope));
    }
                                                                

    BlastEffectiveLengthsOptions* eff_len_opts = NULL;
    BlastEffectiveLengthsOptionsNew(&eff_len_opts);
    BLAST_FillEffectiveLengthsOptions(eff_len_opts, 0, 0, 
                                        &effective_searchsp, 1);
    BlastEffectiveLengthsParameters* eff_len_params = NULL; 
    BlastEffectiveLengthsParametersNew(eff_len_opts, 0, 0, &eff_len_params);

    status = BLAST_CalcEffLengths(program, scoring_opts,
                    eff_len_params, sbp, query_info, NULL);
    if (status > 0) {
        return;
    }

    BOOST_REQUIRE(query_info->contexts[0].eff_searchsp == effective_searchsp);

    eff_len_opts = BlastEffectiveLengthsOptionsFree(eff_len_opts);
    eff_len_params = BlastEffectiveLengthsParametersFree(eff_len_params);

    BlastExtensionParameters* ext_params=NULL;
    BlastExtensionParametersNew(program, ext_options, sbp, query_info, 
                                &ext_params);

    const int kAvgSubjLen = 0;
    BlastHitSavingParameters* hit_params=NULL;
    BlastHitSavingParametersNew(program, hitsaving_opts,
                                sbp, query_info, kAvgSubjLen, 
                                2, &hit_params);

    BlastScoringParameters* scoring_params=NULL;
    BlastScoringParametersNew(scoring_opts, sbp, &scoring_params);

    PSIBlastOptions* psi_options=NULL;
    PSIBlastOptionsNew(&psi_options);

    BlastHSPResults* results =
        Blast_HSPResultsNew(query_info->num_queries);
    BOOST_REQUIRE(results);

    rv = Blast_RedoAlignmentCore(program, query_blk, 
                                        query_info, sbp, NULL, seq_src, 
                                        BLAST_GENETIC_CODE, NULL, hsp_stream,
                                        scoring_params, 
                                        ext_params, hit_params, psi_options, 
                                        results);
    BOOST_REQUIRE_MESSAGE(rv == (Int2)0, "Blast_RedoAlignmentCore failed!");

    hsp_stream = BlastHSPStreamFree(hsp_stream);
    ext_params = BlastExtensionParametersFree(ext_params);
    ext_options = BlastExtensionOptionsFree(ext_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    scoring_params = BlastScoringParametersFree(scoring_params);
    psi_options = PSIBlastOptionsFree(psi_options);
    sbp = BlastScoreBlkFree(sbp);

    BOOST_REQUIRE(results && results->num_queries == 1);
    BOOST_REQUIRE(*results->hitlist_array != NULL);
    BOOST_REQUIRE(results->hitlist_array[0]->hsplist_count > 0);
    BlastHSPList* hsp_list = results->hitlist_array[0]->hsplist_array[0];

    BOOST_REQUIRE_EQUAL(ending_hsp_list->hspcnt, hsp_list->hspcnt);
#if 0

    if ( ending_hsp_list->hspcnt !=  hsp_list->hspcnt) {
        cout << "Expected num hsps=" << ending_hsp_list->hspcnt;
        cout << " Actual num hsps=" << hsp_list->hspcnt << endl;
    }
#endif
    for (int index=0; index<hsp_list->hspcnt; index++)
    {
        BlastHSP* expected_hsp = ending_hsp_list->hsp_array[index];
        BlastHSP* actual_hsp = hsp_list->hsp_array[index];

#if 0
        cout << index << ": query_offset=" 
                << actual_hsp->query.offset << endl;
        cout << index << ": query_end=" 
                << actual_hsp->query.end << endl;
        cout << index << ": subject_offset=" 
                << actual_hsp->subject.offset << endl;
        cout << index << ": subject_end=" 
                << actual_hsp->subject.end << endl;
        cout << index << ": score=" 
                << actual_hsp->score << endl;
        cout << index << ": bit_score=" 
                << actual_hsp->bit_score << endl;
        cout << index << ": evalue=" 
                << actual_hsp->evalue << endl;
        cout << index << ": num_ident=" 
                << actual_hsp->num_ident << endl;
#endif
        BOOST_REQUIRE_EQUAL(expected_hsp->query.offset, 
                                actual_hsp->query.offset);
        BOOST_REQUIRE_EQUAL(expected_hsp->query.end, 
                                actual_hsp->query.end);
        BOOST_REQUIRE_EQUAL(expected_hsp->subject.offset, 
                                actual_hsp->subject.offset);
        BOOST_REQUIRE_EQUAL(expected_hsp->subject.end, 
                                actual_hsp->subject.end);
        BOOST_REQUIRE_EQUAL(expected_hsp->score, 
                                actual_hsp->score);
        BOOST_REQUIRE_EQUAL(expected_hsp->num_ident, 
                                actual_hsp->num_ident);
#if 0
        double diff = fabs((expected_hsp->evalue-actual_hsp->evalue));
        cerr << "Diff in evalues for " << index << "=" << diff << endl;
#endif
        BOOST_REQUIRE_CLOSE(expected_hsp->evalue, actual_hsp->evalue, 10.0);
        //    cout << "HSP " << index << " OK" << endl;
    }

    results = Blast_HSPResultsFree(results);
}

BOOST_FIXTURE_TEST_SUITE(RedoAlignment, CRedoAlignmentTestFixture)

// Start modularized code
BOOST_AUTO_TEST_CASE(testRedoAlignmentWithCompBasedStats) {
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    const int k_num_hsps_start = 3;
    const int k_num_hsps_end = 2;
    CSeq_id query_id("gi|3091");
    CSeq_id subj_id("gi|402871");

    // It seems that the last hsp in this set was manually constructed and
    // expected to be dropped (please note that the first 2 hsps we
    // constructed using blastall (thus filtering on), but the sequence
    // passed to RedoAlignmentCore was unfiltered (default for blastpgp)
    const int query_offset[k_num_hsps_start] = { 28, 46, 463};
    const int query_end[k_num_hsps_start] = { 485, 331, 488};
    const int subject_offset[k_num_hsps_start] = { 36, 327, 320};
    const int subject_end[k_num_hsps_start] = { 512, 604, 345};
    const int score[k_num_hsps_start] = { 554, 280, 28};
    const int query_gapped_start[k_num_hsps_start] = { 431, 186, 480};
    const int subject_gapped_start[k_num_hsps_start] = { 458, 458, 337};

    // This is freed by the HSPStream interface
    BlastHSPList* init_hsp_list = 
        setUpHSPList(k_num_hsps_start,
                                                query_offset, query_end,
                                                subject_offset, subject_end,
                                                query_gapped_start, 
                                                subject_gapped_start,
                                                score);

    const int query_offset_final[k_num_hsps_end] = { 2, 46};
    const int query_end_final[k_num_hsps_end] = { 485, 331};
    const int subject_offset_final[k_num_hsps_end] = { 9, 327};
    const int subject_end_final[k_num_hsps_end] = { 512, 604};
    const int score_final[k_num_hsps_end] = { 510, 282};
    const double evalue_final[k_num_hsps_end] = {3.9816e-60, 1.9935e-30};
    const int num_idents_final[k_num_hsps_end] = { 171, 94 };

    BlastHSPList* ending_hsp_list = 
        setUpHSPList(k_num_hsps_end,
                                                query_offset_final,
                                                query_end_final,
                                                subject_offset_final,
                                                subject_end_final,
                                                query_offset_final,
                                                subject_offset_final,
                                                score_final, 
                                                evalue_final,
                                                num_idents_final);

    const Int8 kEffSearchSp = 500000;
    const bool kSmithWaterman = false;

    runRedoAlignmentCoreUnitTest(kProgram, query_id, subj_id,
                                    init_hsp_list, ending_hsp_list,
                                    kEffSearchSp, eCompositionBasedStats,
                                    kSmithWaterman);

    ending_hsp_list = Blast_HSPListFree(ending_hsp_list);
}

BOOST_AUTO_TEST_CASE(testRedoAlignmentWithConditionalAdjust) {
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    const int k_num_hsps_start = 3;
    const int k_num_hsps_end = 2;
    CSeq_id query_id("gi|3091");
    CSeq_id subj_id("gi|402871");

    // It seems that the last hsp in this set was manually constructed and
    // expected to be dropped (please note that the first 2 hsps we
    // constructed using blastall (thus filtering on), but the sequence
    // passed to RedoAlignmentCore was unfiltered (default for blastpgp)
    const int query_offset[k_num_hsps_start] = { 28, 46, 463};
    const int query_end[k_num_hsps_start] = { 485, 331, 488};
    const int subject_offset[k_num_hsps_start] = { 36, 327, 320};
    const int subject_end[k_num_hsps_start] = { 512, 604, 345};
    const int score[k_num_hsps_start] = { 554, 280, 28};
    const int query_gapped_start[k_num_hsps_start] = { 431, 186, 480};
    const int subject_gapped_start[k_num_hsps_start] = { 458, 458, 337};

    // This is freed by the HSPStream interface
    BlastHSPList* init_hsp_list = 
        setUpHSPList(k_num_hsps_start,
                                                query_offset, query_end,
                                                subject_offset, subject_end,
                                                query_gapped_start, 
                                                subject_gapped_start,
                                                score);

    const int query_offset_final[k_num_hsps_end] = { 2, 46};
    const int query_end_final[k_num_hsps_end] = { 517, 331};
    const int subject_offset_final[k_num_hsps_end] = { 9, 327};
    const int subject_end_final[k_num_hsps_end] = { 546, 604};
    const int score_final[k_num_hsps_end] = { 537, 298};
    const double evalue_final[k_num_hsps_end] = {8.6649e-64, 1.8159e-32};
    const int num_idents_final[k_num_hsps_end] = { 177, 95 };

    BlastHSPList* ending_hsp_list = 
        setUpHSPList(k_num_hsps_end,
                                                query_offset_final,
                                                query_end_final,
                                                subject_offset_final,
                                                subject_end_final,
                                                query_offset_final,
                                                subject_offset_final,
                                                score_final, 
                                                evalue_final,
                                                num_idents_final);

    const Int8 kEffSearchSp = 500000;
    const bool kSmithWaterman = false;

    runRedoAlignmentCoreUnitTest(kProgram, query_id, subj_id,
                                    init_hsp_list, ending_hsp_list,
                                    kEffSearchSp, eCompositionMatrixAdjust,
                                    kSmithWaterman);

    ending_hsp_list = Blast_HSPListFree(ending_hsp_list);
}

BOOST_AUTO_TEST_CASE(testPSIRedoAlignmentWithCompBasedStats) {
    const EBlastProgramType kProgram = eBlastTypePsiBlast;
    const int k_num_hsps_start = 6;
    const int k_num_hsps_end = 2;
    CSeq_id query_id("gi|129295");
    CSeq_id subj_id("gi|7450545");

    const int query_offset[k_num_hsps_start] = { 24, 99, 16, 84, 6, 223 };
    const int query_end[k_num_hsps_start] = { 62, 128, 24, 114, 25, 231 };
    const int subject_offset[k_num_hsps_start] = 
    { 245, 0, 198, 86, 334, 151 };
    const int subject_end[k_num_hsps_start] = 
    { 287, 29, 206, 119, 353, 159 };
    const int score[k_num_hsps_start] = { 37, 26, 25, 25, 24, 24 };
    const int query_gapped_start[k_num_hsps_start] = 
    { 29, 104, 20, 91, 19, 227 };
    const int subject_gapped_start[k_num_hsps_start] = 
    { 250, 5, 202, 93, 347, 155 };

    // No gaps were found in these alignments. This is freed by the
    // HSPStream interface
    BlastHSPList* init_hsp_list = 
        setUpHSPList(k_num_hsps_start,
                                                query_offset, query_end,
                                                subject_offset, subject_end,
                                                query_gapped_start, 
                                                subject_gapped_start,
                                                score);

    const int query_offset_final[k_num_hsps_end] = { 24, 18 };
    const int query_end_final[k_num_hsps_end] = { 30, 31 };
    const int subject_offset_final[k_num_hsps_end] = { 245, 200 };
    const int subject_end_final[k_num_hsps_end] = { 251, 210 };
    const int score_final[k_num_hsps_end] = { 29, 24 };
    const double evalue_final[k_num_hsps_end] = 
    { 1.361074 , 6.425098 };
    const int ident_final[k_num_hsps_end] = { 3, 6};
            

    BlastHSPList* ending_hsp_list = 
        setUpHSPList(k_num_hsps_end,
                                                query_offset_final,
                                                query_end_final,
                                                subject_offset_final,
                                                subject_end_final,
                                                query_offset_final,
                                                subject_offset_final,
                                                score_final, 
                                                evalue_final,
                                                ident_final);


    const Int8 kEffSearchSp = 84660;
    const bool kSmithWaterman = false;

    runRedoAlignmentCoreUnitTest(kProgram, query_id, subj_id,
                                    init_hsp_list, ending_hsp_list,
                                    kEffSearchSp, eCompositionBasedStats,
                                    kSmithWaterman);
    ending_hsp_list = Blast_HSPListFree(ending_hsp_list);
}

BOOST_AUTO_TEST_CASE(testRedoAlignmentWithCompBasedStatsBadlyBiasedSequence) {
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    const int k_num_hsps_start = 6;
    const int k_num_hsps_end = 5;
    // CSeq_id query_id("gi|129295");
    CSeq_id subj_id("gb|AAA22059|");
    auto_ptr<SSeqLoc> ssl(CTestObjMgr::Instance().CreateSSeqLoc(subj_id));

    CNcbiIfstream infile("data/biased.fsa");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastFastaInputSource> fasta_src
        (new CBlastFastaInputSource(infile, iconfig));
    CRef<CBlastInput> input(new CBlastInput(&*fasta_src));
    CRef<CScope> scope = CBlastScopeSource(is_protein).NewScope();

    blast::TSeqLocVector query_seqs = input->GetAllSeqLocs(*scope);

    const int query_offset[k_num_hsps_start] = { 3, 1, 4, 3, 0, 1 };
    const int query_end[k_num_hsps_start] = { 236, 232, 236, 235, 226, 233 };
    const int subject_offset[k_num_hsps_start] = 
    { 1, 1, 6, 6, 12, 22 };
    const int subject_end[k_num_hsps_start] = 
    { 238, 238, 238, 238, 238, 254 };
    const int score[k_num_hsps_start] = { 345, 344, 343, 339, 332, 320 };
    const int query_gapped_start[k_num_hsps_start] = 
    { 32, 194, 9, 8, 104, 9 };
    const int subject_gapped_start[k_num_hsps_start] = 
    { 30, 200, 11, 11, 116, 30 };

    // No gaps were found in these alignments. This is freed by the
    // HSPStream interface
    BlastHSPList* init_hsp_list = 
        setUpHSPList(k_num_hsps_start,
                                                query_offset, query_end,
                                                subject_offset, subject_end,
                                                query_gapped_start, 
                                                subject_gapped_start,
                                                score);

    const int query_offset_final[k_num_hsps_end] = { 4, 3, 3, 0, 0};
    const int query_end_final[k_num_hsps_end] = { 236, 235, 220, 226, 232};
    const int subject_offset_final[k_num_hsps_end] = { 6, 6, 1, 12, 6};
    const int subject_end_final[k_num_hsps_end] = { 238, 238, 218, 238, 238};
    const int score_final[k_num_hsps_end] = { 73, 72, 69, 68, 66};
    const double evalue_final[k_num_hsps_end] = 
    { 1.26e-05 , 1.7e-5 , 4.0e-5, 5.1e-5, 0.0000775};
    const int num_idents_final[k_num_hsps_end] = { 87, 85, 81, 84, 85 };
            

    BlastHSPList* ending_hsp_list = 
        setUpHSPList(k_num_hsps_end,
                                                query_offset_final,
                                                query_end_final,
                                                subject_offset_final,
                                                subject_end_final,
                                                query_offset_final,
                                                subject_offset_final,
                                                score_final, 
                                                evalue_final,
                                                num_idents_final);


    const Int8 kEffSearchSp = 84660;
    const bool kSmithWaterman = false;

    runRedoAlignmentCoreUnitTest(kProgram, query_seqs[0], *ssl,
                                    init_hsp_list, ending_hsp_list,
                                    kEffSearchSp, eCompositionMatrixAdjust,
                                    kSmithWaterman);
    ending_hsp_list = Blast_HSPListFree(ending_hsp_list);
}

BOOST_AUTO_TEST_CASE(testRedoAlignmentWithSW) {
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    const int k_num_hsps_start = 3;
    const int k_num_hsps_end = 5;
    CSeq_id query_id("gi|3091");
    CSeq_id subj_id("gi|402871");

    // It seems that the last hsp in this set was manually constructed and
    // expected to be dropped (please note that the first 2 hsps we
    // constructed using blastall (thus filtering on), but the sequence
    // passed to RedoAlignmentCore was unfiltered (default for blastpgp)
    const int query_offset[k_num_hsps_start]         = { 28, 46, 463 };
    const int query_end[k_num_hsps_start]            = { 485, 331, 488 };
    const int subject_offset[k_num_hsps_start]       = { 36, 327, 320 };
    const int subject_end[k_num_hsps_start]          = { 512, 604, 345 };
    const int score[k_num_hsps_start]                = { 554, 280, 28 };
    const int query_gapped_start[k_num_hsps_start]   = { 431, 186, 480 };
    const int subject_gapped_start[k_num_hsps_start] = { 458, 458, 337 };

    // This is freed by the HSPStream interface
    BlastHSPList* init_hsp_list =
        setUpHSPList(k_num_hsps_start,
                                                query_offset, query_end,
                                                subject_offset, subject_end,
                                                query_gapped_start,
                                                subject_gapped_start,
                                                score);

    const int query_offset_final[k_num_hsps_end]   = { 2, 250, 494, 67, 2 };
    const int query_end_final[k_num_hsps_end]      = { 485, 331, 530, 86, 24 };
    const int subject_offset_final[k_num_hsps_end] = { 9, 523, 261, 585, 570 };
    const int subject_end_final[k_num_hsps_end]    = { 512, 604, 297, 604, 592 };    
    const int score_final[k_num_hsps_end]          = { 591, 39, 37, 33, 32 };
    const double evalue_final[k_num_hsps_end]      = { 2.4581e-71, 0.387,      
                                                       0.6692, 1.9988, 2.6256 };
    const int num_idents_final[k_num_hsps_end]     = { 172, 22, 9, 8, 7 };

    BlastHSPList* ending_hsp_list = 
        setUpHSPList(k_num_hsps_end,
                                                query_offset_final,
                                                query_end_final,
                                                subject_offset_final,
                                                subject_end_final,
                                                query_offset_final,
                                                subject_offset_final,
                                                score_final, 
                                                evalue_final,
                                                num_idents_final);

    const Int8 kEffSearchSp = 500000;
    const bool kSmithWaterman = true;

    runRedoAlignmentCoreUnitTest(kProgram, query_id, subj_id,
                                    init_hsp_list, ending_hsp_list,
                                    kEffSearchSp, eNoCompositionBasedStats,
                                    kSmithWaterman);
    ending_hsp_list = Blast_HSPListFree(ending_hsp_list);
}

BOOST_AUTO_TEST_CASE(testRedoAlignmentWithCompBasedStatsAndSW) {
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    const int k_num_hsps_start = 3;
    const int k_num_hsps_end = 3;
    CSeq_id query_id("gi|3091");
    CSeq_id subj_id("gi|402871");

    const int query_offset[k_num_hsps_start] = { 28, 46, 463};
    const int query_end[k_num_hsps_start] = { 485, 331, 488};
    const int subject_offset[k_num_hsps_start] = { 36, 327, 320};
    const int subject_end[k_num_hsps_start] = { 512, 604, 345};
    const int score[k_num_hsps_start] = { 554, 280, 28};
    const int query_gapped_start[k_num_hsps_start] = { 431, 186, 480};
    const int subject_gapped_start[k_num_hsps_start] = { 458, 458, 337};

    // This is freed by the HSPStream interface
    BlastHSPList* init_hsp_list = 
        setUpHSPList(k_num_hsps_start,
                                                query_offset, query_end,
                                                subject_offset, subject_end,
                                                query_gapped_start, 
                                                subject_gapped_start,
                                                score);

    const int query_offset_final[k_num_hsps_end] = { 2, 250, 67 };
    const int query_end_final[k_num_hsps_end] = { 485, 331, 86};
    const int subject_offset_final[k_num_hsps_end] = { 9, 523, 585};
    const int subject_end_final[k_num_hsps_end] = { 512, 604, 604};
    const int score_final[k_num_hsps_end] = { 510, 34, 31};
    const double evalue_final[k_num_hsps_end] = {3.9816e-60, 1.349, 3.7944};
    const int num_idents_final[k_num_hsps_end] = { 171, 22, 8 };

    BlastHSPList* ending_hsp_list = 
        setUpHSPList(k_num_hsps_end,
                                                query_offset_final,
                                                query_end_final,
                                                subject_offset_final,
                                                subject_end_final,
                                                query_offset_final,
                                                subject_offset_final,
                                                score_final, 
                                                evalue_final,
                                                num_idents_final);

    const Int8 kEffSearchSp = 500000;
    const bool kSmithWaterman = true;

    runRedoAlignmentCoreUnitTest(kProgram, query_id, subj_id,
                                    init_hsp_list, ending_hsp_list,
                                    kEffSearchSp, eCompositionBasedStats,
                                    kSmithWaterman);

    ending_hsp_list = Blast_HSPListFree(ending_hsp_list);
}

BOOST_AUTO_TEST_CASE(testPSIRedoAlignmentWithCompBasedStatsAndSW) {
    const EBlastProgramType kProgram = eBlastTypePsiBlast;
    const int k_num_hsps_start = 6;
    const int k_num_hsps_end = 8;
    CSeq_id query_id("gi|129295");
    CSeq_id subj_id("gi|7450545");

    const int query_offset[k_num_hsps_start] = 
        { 24, 99, 16, 84, 6, 223 };
    const int query_end[k_num_hsps_start] = 
        { 62, 128, 24, 114, 25, 231 };
    const int subject_offset[k_num_hsps_start] = 
        { 245, 0, 198, 86, 334, 151 };
    const int subject_end[k_num_hsps_start] = 
        { 287, 29, 206, 119, 353, 159 };
    const int score[k_num_hsps_start] = 
        { 37, 26, 25, 25, 24, 24 };
    const int query_gapped_start[k_num_hsps_start] = 
        { 29, 104, 20, 91, 19, 227 };
    const int subject_gapped_start[k_num_hsps_start] = 
        { 250, 5, 202, 93, 347, 155 };


    // No gaps were found in these alignments. This is freed by the
    // HSPStream interface
    BlastHSPList* init_hsp_list = 
        setUpHSPList(k_num_hsps_start,
                                                query_offset, query_end,
                                                subject_offset, subject_end,
                                                query_gapped_start,
                                                subject_gapped_start,
                                                score);

    const int query_offset_final[k_num_hsps_end] = 
        { 24, 140, 126, 10, 137, 198, 18, 137 };
    const int query_end_final[k_num_hsps_end] = 
        { 30, 171, 205, 35, 157, 208, 31, 152 };
    const int subject_offset_final[k_num_hsps_end] = 
        { 245, 408, 212, 130, 339, 388, 200, 186 };
    const int subject_end_final[k_num_hsps_end] = 
        { 251, 439, 287, 155, 359, 398, 210, 201 };
    const int score_final[k_num_hsps_end] = 
        { 29, 28, 28, 28, 25, 24, 24, 22 };
    const double evalue_final[k_num_hsps_end] = 
        { 1.361074, 1.837947, 2.118044, 2.153685, 4.198304, 5.529096, 
            6.425098, 8.532644 };
    const int ident_final[k_num_hsps_end] = 
        { 3, 8, 23, 10, 6, 5, 6, 5};

    BlastHSPList* ending_hsp_list = 
        setUpHSPList(k_num_hsps_end,
                                                query_offset_final,
                                                query_end_final,
                                                subject_offset_final,
                                                subject_end_final,
                                                query_offset_final,
                                                subject_offset_final,
                                                score_final, 
                                                evalue_final,
                                                ident_final);


    const Int8 kEffSearchSp = 84660;
    const bool kSmithWaterman = true;

    runRedoAlignmentCoreUnitTest(kProgram, query_id, subj_id,
                                    init_hsp_list, ending_hsp_list,
                                    kEffSearchSp, eCompositionBasedStats,
                                    kSmithWaterman);

    ending_hsp_list = Blast_HSPListFree(ending_hsp_list);
}

// Tests following changes from Mike Gertz and Alejandro Schaffer (rev 1.14
// of blast_kappa.c):
//
// For secondary alignments with Smith-Waterman on, use the E-value from
// the X-drop alignment computed by ALIGN that has more flexibility in
// which positions are allowed in the dynamic program.
//
// Add a sort of alignments for the same query-subject pair because the
// use of X-drop alignments occasionally reorders such alignments.
BOOST_AUTO_TEST_CASE(testRedoAlignmentUseXdropEvalue) {
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    const int k_num_hsps_start = 4;
    const int k_num_hsps_end = 4;
    CSeq_id query_id("gi|48100936");
    CSeq_id subj_id("gi|7301132");

    const int query_offset[k_num_hsps_start] = { 995, 1004, 995, 973};
    const int query_end[k_num_hsps_start] = { 1314, 1314, 1403, 1316};
    const int subject_offset[k_num_hsps_start] = { 61, 36, 61, 106};
    const int subject_end[k_num_hsps_start] = { 384, 384, 455, 420};
    const int score[k_num_hsps_start] = { 341, 327, 314, 301};
    const int query_gapped_start[k_num_hsps_start] = { 1233, 1017, 1310, 
                                                        1228};
    const int subject_gapped_start[k_num_hsps_start] = { 303, 49, 347, 331};

    // This is freed by the HSPStream interface
    BlastHSPList* init_hsp_list = 
        setUpHSPList(k_num_hsps_start,
                                                query_offset, query_end,
                                                subject_offset, subject_end,
                                                query_gapped_start, 
                                                subject_gapped_start,
                                                score);
    const int query_offset_final[k_num_hsps_end] =   
        {  995, 1261, 1025, 1210};
    const int query_end_final[k_num_hsps_end] =      
        { 1314, 1341, 1125, 1243};
    const int subject_offset_final[k_num_hsps_end] = 
        {   61,    1,  387,  17};
    const int subject_end_final[k_num_hsps_end] =    
        {  384,  115,  482, 50};
    const int score_final[k_num_hsps_end] =          
        {  323,   78,   69, 60};
    const double evalue_final[k_num_hsps_end] =      
        { 2.712e-34, 3.6003e-05, 0.00048334, 0.00441};  
    const int num_idents_final[k_num_hsps_end] = { 108, 31, 30, 12 };

    BlastHSPList* ending_hsp_list = 
        setUpHSPList(k_num_hsps_end,
                                                query_offset_final,
                                                query_end_final,
                                                subject_offset_final,
                                                subject_end_final,
                                                query_offset_final,
                                                subject_offset_final,
                                                score_final, 
                                                evalue_final,
                                                num_idents_final);

    const Int8 kEffSearchSp = 1000*1000;
    const bool kSmithWaterman = true;

    const int kHitListSize = 1;
    const double kEvalueThreshold = 0.005;

    runRedoAlignmentCoreUnitTest(kProgram, query_id, subj_id,
                                    init_hsp_list, ending_hsp_list,
                                    kEffSearchSp, eCompositionBasedStats,
                                    kSmithWaterman, kEvalueThreshold,
                                    kHitListSize);

    ending_hsp_list = Blast_HSPListFree(ending_hsp_list);
}

// N.B.: the absence of a testPSIRedoAlignmentWithSW is due to the fact
// that blastpgp reads frequency rations from a checkpoint file and scales
// the PSSM and K parameter, which we cannot do unless we support reading
// checkpoint files. QA should be added later 

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: redoalignment-cppunit.cpp,v $
* Revision 1.59  2009/04/27 15:15:41  camacho
* Bring up to date with latest filtering strategy changes JIRA SB-170
*
* Revision 1.55  2009/03/13 18:15:23  maning
* Use the writer/pipe framework to construct hsp_stream.  Removed queue test.
*
* Revision 1.58  2009/03/13 19:25:56  maning
* Previous commit messed up.  Roll back again.
*
* Revision 1.56  2009/03/13 18:45:15  maning
* Roll back to previous version.
*
* Revision 1.54  2008/11/21 21:13:27  madden
* tests for num_ident fix
*
* Revision 1.53  2008/10/27 17:00:12  camacho
* Fix include paths to deprecated headers
*
* Revision 1.52  2008/02/13 21:39:12  camacho
* Re-enable choice to sort by score to meet pre-condition of composition-based
* statistics code.
*
* Revision 1.51  2007/11/19 18:48:34  camacho
*  Bring up to date with changes in SVN revision 114090
*
* Revision 1.50  2007/11/08 17:09:08  camacho
* Bring up to date with changes in SVN revision 113729
*
* Revision 1.49  2007/10/22 19:16:10  madden
* BlastExtensionOptionsNew has Boolean gapped arg
*
* Revision 1.48  2007/07/27 18:04:34  papadopo
* change signature of HSPListCollector_Init
*
* Revision 1.47  2007/05/08 22:41:31  papadopo
* change signature of RedoAlignmentCore
*
* Revision 1.46  2007/03/22 14:34:44  camacho
* + support for auto-detection of genetic codes
*
* Revision 1.45  2007/03/20 14:54:02  camacho
* changes related to addition of multiple genetic code specification
*
* Revision 1.44  2006/11/21 17:47:50  papadopo
* use enum for lookup table type
*
* Revision 1.43  2006/09/25 19:34:31  madden
*   Changed values to reflect the results of computations done with
*   more precise frequencies and frequency ratios.
*   [from Mike Gertz]
*
* Revision 1.42  2006/06/29 16:25:24  camacho
* Changed BlastHitSavingOptionsNew signature
*
* Revision 1.41  2006/06/05 13:34:05  madden
* Changes to remove [GS]etMatrixPath and use callback instead
*
* Revision 1.40  2006/05/18 16:32:03  papadopo
* change signature of BLAST_CalcEffLengths
*
* Revision 1.39  2006/01/23 19:57:52  camacho
* Allow new varieties of composition based statistics
*
* Revision 1.38  2005/12/16 20:51:50  camacho
* Diffuse the use of CSearchMessage, TQueryMessages, and TSearchMessages
*
* Revision 1.37  2005/12/01 15:13:52  madden
* replaced Kappa_RedoAlignmentCore with Blast_RedoAlignmentCore
*
* Revision 1.36  2005/10/14 13:47:32  camacho
* Fixes to pacify icc compiler
*
* Revision 1.35  2005/06/09 20:37:06  camacho
* Use new private header blast_objmgr_priv.hpp
*
* Revision 1.34  2005/05/24 20:05:17  camacho
* Changed signature of SetupQueries and SetupQueryInfo
*
* Revision 1.33  2005/05/23 15:53:19  dondosha
* Special case for preliminary hitlist size in RPS BLAST - hence no need for 2 extra parameters in SBlastHitsParametersNew
*
* Revision 1.32  2005/05/19 17:10:04  madden
* Adjust an expect value, use BOOST_REQUIRE_CLOSE when appropriate
*
* Revision 1.31  2005/05/16 12:29:15  madden
* use SBlastHitsParameters in Blast_HSPListCollectorInit and Blast_HSPListCollectorInit[MT]
*
* Revision 1.30  2005/04/27 20:08:40  dondosha
* PHI-blast boolean argument has been removed from BlastSetup_ScoreBlkInit
*
* Revision 1.29  2005/04/06 21:27:18  dondosha
* Use EBlastProgramType instead of EProgram in internal functions
*
* Revision 1.28  2005/04/06 16:27:46  camacho
* Fix to previous commit
*
* Revision 1.27  2005/03/31 20:46:44  camacho
* Changes to avoid making CBlastRedoAlignmentTest a friend of CBlastOptions
*
* Revision 1.26  2005/03/31 13:45:58  camacho
* BLAST options API clean-up
*
* Revision 1.25  2005/03/04 17:20:45  bealer
* - Command line option support.
*
* Revision 1.24  2005/02/16 14:52:41  camacho
* Fix memory leak
*
* Revision 1.23  2005/02/14 14:17:17  camacho
* Changes to use SBlastScoreMatrix
*
* Revision 1.22  2005/01/06 15:43:25  camacho
* Make use of modified signature to blast::SetupQueries
*
* Revision 1.21  2004/12/14 21:16:31  dondosha
* Query frame argument added to Blast_HSPInit; renamed all constants according to toolkit convention
*
* Revision 1.20  2004/12/09 15:24:10  dondosha
* BlastSetup_GetScoreBlock renamed to BlastSetup_ScoreBlkInit
*
* Revision 1.19  2004/12/02 16:12:49  bealer
* - Change multiple-arrays to array-of-struct in BlastQueryInfo
*
* Revision 1.18  2004/11/23 21:51:34  camacho
* Update call to Kappa_RedoAlignmentCore as its signature has changed.
* num_ident field of HSP structure is no longer populated in
* Kappa_RedoAlignmentCore.
*
* Revision 1.17  2004/11/17 21:02:01  camacho
* Add error checking to BlastSeqSrc initialization
*
* Revision 1.16  2004/11/09 15:16:26  camacho
* Minor change in auxiliary function
*
* Revision 1.15  2004/11/02 22:08:30  camacho
* Refactored main body of unit test to be reused by multiple tests
*
* Revision 1.14  2004/11/02 18:28:52  madden
* BlastHitSavingParametersNew no longer requires BlastExtensionParameters
*
* Revision 1.13  2004/11/01 18:38:30  madden
* Change call to BLAST_FillHitSavingOptions
*
* Revision 1.12  2004/10/29 14:19:20  camacho
* Make BlastHSPResults allocation function follow naming conventions
*
* Revision 1.11  2004/10/14 17:14:28  madden
* New parameter in BlastHitSavingParametersNew
*
* Revision 1.10  2004/07/19 15:04:22  dondosha
* Renamed multiseq_src to seqsrc_multiseq, seqdb_src to seqsrc_seqdb
*
* Revision 1.9  2004/07/06 15:58:45  dondosha
* Use EBlastProgramType enumeration type for program when calling C functions
*
* Revision 1.8  2004/06/22 16:46:19  camacho
* Changed the blast_type_* definitions for the EBlastProgramType enumeration.
*
* Revision 1.7  2004/06/21 20:29:07  camacho
* Fix memory leaks, others remain
*
* Revision 1.6  2004/06/21 14:23:32  madden
* Add test testRedoAlignmentUseXdropEvalue that tests recent changes by Mike Gertz to kappa.c ported to blast_kappa.c
*
* Revision 1.5  2004/06/21 13:06:51  madden
* Add check for expect value
*
* Revision 1.4  2004/06/21 12:54:49  camacho
* CVS Id tag fix
*
* Revision 1.3  2004/06/08 15:24:34  dondosha
* Use BlastHSPStream interface
*
* Revision 1.2  2004/05/19 17:10:35  madden
* Fix memory leaks
*
* Revision 1.1  2004/05/18 17:17:45  madden
* Tests for composition-based stats and smith-waterman
*
*
*
* ===========================================================================
*/
