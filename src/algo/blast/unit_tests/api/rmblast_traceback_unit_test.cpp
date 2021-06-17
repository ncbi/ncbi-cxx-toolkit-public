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
* Author:  Robert Hubley ( boiler plate: Tom Madden )
*
* File Description:
*   Unit test module for traceback calculation in RMBlast
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

#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_traceback.h>

#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blastx_options.hpp>
#include <algo/blast/api/tblastn_options.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/lookup_util.h>
#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/hspfilter_collector.h>
#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <algo/blast/api/blast_types.hpp>

#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/traceback_stage.hpp>

#include <algo/blast/api/blast_seqinfosrc.hpp>
#include <algo/blast/api/seqinfosrc_seqdb.hpp>

#include "test_objmgr.hpp"
#include "blast_test_util.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

class CTracebackTestFixture {

public:
    BlastScoreBlk* m_ScoreBlk;
    BlastSeqLoc* m_LookupSegments;
    Blast_Message* m_BlastMessage;
    BlastScoringParameters* m_ScoreParams;
    BlastExtensionParameters* m_ExtParams;
    BlastHitSavingParameters* m_HitParams;
    BlastEffectiveLengthsParameters* m_EffLenParams;
    BlastGapAlignStruct* m_GapAlign;


    CTracebackTestFixture() {
        m_ScoreBlk=NULL;
        m_LookupSegments=NULL;
        m_BlastMessage=NULL;
        m_ScoreParams = NULL;
        m_ExtParams=NULL;
        m_HitParams=NULL;
        m_EffLenParams=NULL;
        m_GapAlign=NULL;
    }

    ~CTracebackTestFixture() {
        m_LookupSegments = BlastSeqLocFree(m_LookupSegments);
        m_BlastMessage = Blast_MessageFree(m_BlastMessage);
        m_ScoreBlk = BlastScoreBlkFree(m_ScoreBlk);
        m_ScoreParams = BlastScoringParametersFree(m_ScoreParams);
        m_HitParams = BlastHitSavingParametersFree(m_HitParams);
        m_ExtParams = BlastExtensionParametersFree(m_ExtParams);
        m_EffLenParams = BlastEffectiveLengthsParametersFree(m_EffLenParams);
        m_GapAlign = BLAST_GapAlignStructFree(m_GapAlign);
    }

    static BlastHSPStream* x_MakeStream(const CBlastOptions &opt) {
        BlastHSPWriterInfo * writer_info = BlastHSPCollectorInfoNew(
                  BlastHSPCollectorParamsNew(opt.GetHitSaveOpts(), 
                                             opt.GetExtnOpts()->compositionBasedStats, 
                                             opt.GetScoringOpts()->gapped_calculation));

        BlastHSPWriter* writer = BlastHSPWriterNew(&writer_info, NULL, NULL);
        BOOST_REQUIRE(writer_info == NULL);
        return BlastHSPStreamNew(opt.GetProgramType(), opt.GetExtnOpts(), 
                                 FALSE, 1, writer);
    }

    void x_SetupMain(const CBlastOptions &opt, 
                     const CBLAST_SequenceBlk &query_blk,
                     const CBlastQueryInfo &query_info) {
        BLAST_MainSetUp(opt.GetProgramType(),
                        opt.GetQueryOpts(), 
                        opt.GetScoringOpts(),
                        query_blk, query_info, 1.0, &m_LookupSegments, NULL, 
                        &m_ScoreBlk, &m_BlastMessage, &BlastFindMatrixPath);
    }

    void x_SetupGapAlign(const CBlastOptions &opt,
                         const BlastSeqSrc* seq_src,
                         const CBlastQueryInfo &query_info) {
        BLAST_GapAlignSetUp(opt.GetProgramType(), seq_src,
                            opt.GetScoringOpts(),
                            opt.GetEffLenOpts(),
                            opt.GetExtnOpts(),
                            opt.GetHitSaveOpts(),
                            query_info, m_ScoreBlk, &m_ScoreParams,
                            &m_ExtParams, &m_HitParams, &m_EffLenParams, 
                            &m_GapAlign);
    }
 
    void x_ComputeTracebak(const CBlastOptions &opt,
                           BlastHSPStream *hsp_stream,
                           const CBLAST_SequenceBlk &query_blk,
                           const CBlastQueryInfo &query_info,
                           const BlastSeqSrc* seq_src,
                           BlastHSPResults** results) {
        BLAST_ComputeTraceback(opt.GetProgramType(), 
                               hsp_stream, query_blk, query_info, seq_src, 
                               m_GapAlign, m_ScoreParams, m_ExtParams, m_HitParams, m_EffLenParams,
                               opt.GetDbOpts(), NULL, NULL, NULL, results, 0, 0);
    }

};

BOOST_FIXTURE_TEST_SUITE(traceback, CTracebackTestFixture)


/* Perform an RMBlast style traceback with complexity adjustment and score cutoff
 * -RMH- */
BOOST_AUTO_TEST_CASE(testRMBlastTraceBack) {
     const int k_num_hsps_start = 7;
     const int k_num_hsps_end = 6;

     // Look for the custom matrix 20p43g.matrix in the unit test data directory
     CAutoEnvironmentVariable a("BLASTMAT", "data");

     CSeq_id qid("gi|1945388");
     unique_ptr<SSeqLoc> qsl(
         CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
     CSeq_id sid("gi|1732684");
     unique_ptr<SSeqLoc> ssl(
         CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    CRef<CBlastNucleotideOptionsHandle> opts(new CBlastNucleotideOptionsHandle);
    opts->SetTraditionalMegablastDefaults();
    opts->SetGappedMode(true);
    opts->SetMatchReward(0);
    opts->SetMismatchPenalty(0);
    // NOTE: Unfortunately for the time being the 
    //       blast_state::Blast_KarlinBlkNuclGappedCalc()
    //       function limits our use gap (open/ext) scores to values 
    //       greater or equal to 2. Use of lower values 
    //       causes RMBlast to abort.
    opts->SetGapOpeningCost(12);
    opts->SetGapExtensionCost(2);
    opts->SetWordSize(11);
    opts->SetCutoffScore(500);
    opts->SetGapTracebackAlgorithm(eDynProgTbck);
    opts->SetMatrixName("20p43g.matrix");
    opts->SetComplexityAdjMode( true );

     CBl2Seq blaster(*qsl, *ssl, *opts);

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
         BOOST_REQUIRE(m->empty());
     }

     BlastSeqSrc* seq_src =
         MultiSeqBlastSeqSrcInit(
                 const_cast<TSeqLocVector&>(blaster.GetSubjects()),
                 blaster.GetOptionsHandle().GetOptions().GetProgramType());
     TestUtil::CheckForBlastSeqSrcErrors(seq_src);


     BlastHSPList* hsp_list =
         (BlastHSPList*) calloc(1, sizeof(BlastHSPList));
     hsp_list->oid = 0;
     hsp_list->hspcnt = k_num_hsps_start;
     hsp_list->allocated = k_num_hsps_start;
     hsp_list->hsp_max = k_num_hsps_start;
     hsp_list->do_not_reallocate = FALSE;
     hsp_list->hsp_array = (BlastHSP**) malloc(hsp_list->allocated*sizeof(BlastHSP*));

     BlastHSPStream* hsp_stream = x_MakeStream(blaster.GetOptionsHandle().GetOptions());

     // gapopen 12 gapextend 2
     // word_size = 11
     // matrix = nt/20p43g.matrix
     // complexity adjust
     // minscore = 500  ( removes one hsp with a complexity adj score of 414 )
     const int query_offset[k_num_hsps_start] = { 6378, 5950, 5295, 7191, 7351, 5199, 3818 };
     const int query_end[k_num_hsps_start] = { 6792, 6161, 5400, 7239, 7431, 5276, 3830 };
     const int subject_offset[k_num_hsps_start] = { 4, 39, 16, 378, 1, 0, 71};
     const int subject_end[k_num_hsps_start] = { 419, 241, 123, 428, 86, 66, 83};
     const int score[k_num_hsps_start] = { 1582, 1391, 902, 341, 332, 286, 118 };
     const int context[k_num_hsps_start] = { 0, 0, 0, 0, 1, 0, 1 };
     const int subject_frame[k_num_hsps_start] = { 1, 1, 1, 1, 1, 1, 1 };
     const int query_gapped_start[k_num_hsps_start] = { 6625, 6035, 5298, 7194, 7411, 5202, 3821 };
     const int subject_gapped_start[k_num_hsps_start] = { 244, 116, 19, 381, 66, 3, 74 };


    for (int index=0; index<k_num_hsps_start; index++)
    {
         hsp_list->hsp_array[index] = (BlastHSP*) calloc(1, sizeof(BlastHSP));
         hsp_list->hsp_array[index]->query.offset =query_offset[index];
         hsp_list->hsp_array[index]->query.end =query_end[index];
         hsp_list->hsp_array[index]->subject.offset =subject_offset[index];
         hsp_list->hsp_array[index]->subject.end =subject_end[index];
         hsp_list->hsp_array[index]->score =score[index];
         hsp_list->hsp_array[index]->context =context[index];
         hsp_list->hsp_array[index]->subject.frame =subject_frame[index];
         hsp_list->hsp_array[index]->query.gapped_start =query_gapped_start[index];
         hsp_list->hsp_array[index]->subject.gapped_start =subject_gapped_start[index];
    }

    // needed after tie-breaking algorithm for scores was changed in
    // ScoreCompareHSP (blast_hits.c, revision 1.139)
    Blast_HSPListSortByScore(hsp_list);
    BlastHSPStreamWrite(hsp_stream, &hsp_list);

    x_SetupMain(blaster.GetOptionsHandle().GetOptions(), query_blk, query_info);

    // Set "database" length option to the length of subject sequence,
    // to avoid having to calculate cutoffs and effective lengths twice.
    Int4 oid = 0;
    Uint4 subj_length = BlastSeqSrcGetSeqLen(seq_src, (void*)&oid);
    blaster.SetOptionsHandle().SetDbLength(subj_length);

    x_SetupGapAlign(blaster.GetOptionsHandle().GetOptions(), seq_src, query_info);

    BlastHSPResults* results = NULL;

    x_ComputeTracebak(blaster.GetOptionsHandle().GetOptions(),
                      hsp_stream, query_blk, query_info, seq_src, &results);

    BlastHSPStreamFree(hsp_stream);

    const int query_offset_final[k_num_hsps_end] = { 6374, 5916, 5263, 6778, 5199, 3740 };
    const int query_end_final[k_num_hsps_end] = { 6785, 6345, 5756, 7241, 5522, 4113 };
    const int subject_offset_final[k_num_hsps_end] = { 1, 0, 1, 0, 0, 0 };
    const int subject_end_final[k_num_hsps_end] = { 426, 419, 426, 430, 426, 425 };
    const int score_final[k_num_hsps_end] = { 1560, 1532, 1183, 815, 637, 590 };
    const int context_final[k_num_hsps_end] = { 0, 0, 0, 0, 0, 1 };
    const int subject_frame_final[k_num_hsps_end] = { 1, 1, 1, 1, 1, 1};
    const int query_gapped_start_final[k_num_hsps_end] = { 6625, 6035, 5298, 7194, 5202, 3821 };
    const int subject_gapped_start_final[k_num_hsps_end] = { 244, 116, 19, 381, 3, 74 };
    const int num_ident_final[k_num_hsps_end] = { 314, 304, 300, 273, 238, 247 };

    // One hsp is dropped when the function runs.
    BlastHitList* hit_list = results->hitlist_array[0];
    hsp_list = hit_list->hsplist_array[0];

    BOOST_REQUIRE(hsp_list != NULL);
    BOOST_REQUIRE_EQUAL(k_num_hsps_end, hsp_list->hspcnt);
    for (int index=0; index<k_num_hsps_end; index++)
    {
         BlastHSP* tmp_hsp = hsp_list->hsp_array[index];
         BOOST_REQUIRE_EQUAL(query_offset_final[index], tmp_hsp->query.offset);
         BOOST_REQUIRE_EQUAL(query_end_final[index], tmp_hsp->query.end);
         BOOST_REQUIRE_EQUAL(subject_offset_final[index], tmp_hsp->subject.offset);
         BOOST_REQUIRE_EQUAL(subject_end_final[index], tmp_hsp->subject.end);
         BOOST_REQUIRE_EQUAL(score_final[index], tmp_hsp->score);
         BOOST_REQUIRE_EQUAL(context_final[index], (int) tmp_hsp->context);
         BOOST_REQUIRE_EQUAL(subject_frame_final[index], (int) tmp_hsp->subject.frame);
         BOOST_REQUIRE_EQUAL(query_gapped_start_final[index], tmp_hsp->query.gapped_start);
         BOOST_REQUIRE_EQUAL(subject_gapped_start_final[index], tmp_hsp->subject.gapped_start);
         BOOST_REQUIRE_EQUAL(num_ident_final[index], tmp_hsp->num_ident);
    }

    results = Blast_HSPResultsFree(results);
    BOOST_REQUIRE(results == NULL);
    seq_src = BlastSeqSrcFree(seq_src);
    BOOST_REQUIRE(seq_src == NULL);
}

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log:$
*
* ===========================================================================
*/
