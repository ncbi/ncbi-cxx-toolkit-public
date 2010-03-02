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

        BlastHSPWriter* writer = BlastHSPWriterNew(&writer_info, NULL);
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

/* Checks that HSP data is updated correctly with traceback information. */
BOOST_AUTO_TEST_CASE(testHSPUpdateWithTraceback) {
    const int kOffset=10;
    BlastHSP* new_hsp = Blast_HSPNew();
    BlastGapAlignStruct* gap_align = 
       (BlastGapAlignStruct*) calloc(1, sizeof(BlastGapAlignStruct));
    gap_align->query_start   = kOffset;
    gap_align->query_stop    = 2*kOffset;
    gap_align->subject_start = 3*kOffset;
    gap_align->subject_stop  = 4*kOffset;
    gap_align->score         = 10*kOffset;
    gap_align->edit_script = (GapEditScript*) calloc(1, sizeof(GapEditScript));

    Blast_HSPUpdateWithTraceback(gap_align, new_hsp);

    BOOST_REQUIRE_EQUAL((GapEditScript*) 0, gap_align->edit_script); // this was NULL'ed out
    BOOST_REQUIRE(new_hsp->gap_info); // this got the pointer to edit_script
    BOOST_REQUIRE_EQUAL(kOffset, new_hsp->query.offset);
    BOOST_REQUIRE_EQUAL(2*kOffset, new_hsp->query.end);
    BOOST_REQUIRE_EQUAL(3*kOffset, new_hsp->subject.offset);
    BOOST_REQUIRE_EQUAL(4*kOffset, new_hsp->subject.end);
    BOOST_REQUIRE_EQUAL(10*kOffset, new_hsp->score);

    new_hsp = Blast_HSPFree(new_hsp);
    BOOST_REQUIRE(new_hsp == NULL);
    gap_align = BLAST_GapAlignStructFree(gap_align);
}

BOOST_AUTO_TEST_CASE(testBLASTNTraceBack) {
     const int k_num_hsps_start = 8;
     const int k_num_hsps_end = 7;

     CSeq_id qid("gi|1945388");
     auto_ptr<SSeqLoc> qsl(
         CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
     CSeq_id sid("gi|1732684");
     auto_ptr<SSeqLoc> ssl(
         CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

     CBl2Seq blaster(*qsl, *ssl, eBlastn);

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

     const int query_offset[k_num_hsps_start] = { 6022, 6622, 6622, 5295, 5199, 7191, 3818, 7408};
     const int query_end[k_num_hsps_start] = { 6161, 6730, 6753, 5386, 5219, 7227, 3830, 7419};
     const int subject_offset[k_num_hsps_start] = { 104, 241, 241, 16, 0, 378, 71, 63};
     const int subject_end[k_num_hsps_start] = { 241, 350, 376, 107, 20, 415, 83, 74};
     const int score[k_num_hsps_start] = { 115, 93, 91, 91, 20, 17, 12, 11};
     const int context[k_num_hsps_start] = { 0, 0, 0, 0, 0, 0, 1, 1};
     const int subject_frame[k_num_hsps_start] = { 1, 1, 1, 1, 1, 1, 1, 1};
     const int query_gapped_start[k_num_hsps_start] = { 6035, 6625, 6745, 5295, 5199, 7193, 3819, 7409};
     const int subject_gapped_start[k_num_hsps_start] = { 116, 244, 368, 16, 0, 380, 72, 64};

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

    const int query_offset_final[k_num_hsps_end] = { 6022, 6622, 5295, 7191, 5199, 7396, 3818};
    const int query_end_final[k_num_hsps_end] = { 6161, 6759, 5386, 7231, 5219, 7425, 3830};
    const int subject_offset_final[k_num_hsps_end] = { 104, 241, 16, 378, 0, 51, 71};
    const int subject_end_final[k_num_hsps_end] = { 241, 383, 107, 419, 20, 80, 83};
    const int score_final[k_num_hsps_end] = { 252, 226, 182, 54, 40, 26, 24};
    const int context_final[k_num_hsps_end] = { 0, 0, 0, 0, 0, 1, 1};
    const int subject_frame_final[k_num_hsps_end] = { 1, 1, 1, 1, 1, 1, 1};
    const int query_gapped_start_final[k_num_hsps_end] = { 6035, 6625, 5295, 7193, 5199, 7409, 3819};
    const int subject_gapped_start_final[k_num_hsps_end] = { 116, 244, 16, 380, 0, 64, 72};
    const int num_ident_final[k_num_hsps_end] = { 135, 134, 91, 36, 20, 25, 12};

    // One hsp is dropped when the function runs.
    BlastHitList* hit_list = results->hitlist_array[0];
    hsp_list = hit_list->hsplist_array[0];

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
    seq_src = BlastSeqSrcFree(seq_src);
}

BOOST_AUTO_TEST_CASE(testBLASTNTraceBackLargeXDrop) {
     const int k_num_hsps_start = 3;
     const int k_num_hsps_end = 1;

     CSeq_id qid("gi|42254502");
     auto_ptr<SSeqLoc> qsl(
         CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
     CSeq_id sid("gi|34787366");
     auto_ptr<SSeqLoc> ssl(
         CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

     CBlastNucleotideOptionsHandle opts_handle;
     opts_handle.SetTraditionalBlastnDefaults();
     opts_handle.SetMatchReward(2);
     opts_handle.SetGapXDropoffFinal(200);

     CBl2Seq blaster(*qsl, *ssl, opts_handle);

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
                     opts_handle.GetOptions().GetProgramType());
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

     const int query_offset[k_num_hsps_start] = { 25194, 13986, 22457};
     const int query_end[k_num_hsps_start] = { 31512, 17712, 25019};
     const int subject_offset[k_num_hsps_start] = {11211, 0, 8471}; 
     const int subject_end[k_num_hsps_start] = { 17529, 3726, 11036};
     const int score[k_num_hsps_start] = { 12433, 7421, 4870};
     const int context[k_num_hsps_start] = { 1, 1, 1};
     const int subject_frame[k_num_hsps_start] = { 1, 1, 1};
     const int query_gapped_start[k_num_hsps_start] = { 26671, 13986, 23372};
     const int subject_gapped_start[k_num_hsps_start] = { 12688, 0, 9388};

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

    const int query_offset_final[k_num_hsps_end] = { 13986};
    const int query_end_final[k_num_hsps_end] = { 41877};
    const int subject_offset_final[k_num_hsps_end] = { 0};
    const int subject_end_final[k_num_hsps_end] = { 27888};
    const int score_final[k_num_hsps_end] = { 55540};
    const int context_final[k_num_hsps_end] = { 1};
    const int subject_frame_final[k_num_hsps_end] = { 1};
    const int query_gapped_start_final[k_num_hsps_end] = { 26671};
    const int subject_gapped_start_final[k_num_hsps_end] = { 12688};
    const int num_ident_final[k_num_hsps_end] = { 27856};

    // One hsp is dropped when the function runs.
    BlastHitList* hit_list = results->hitlist_array[0];
    hsp_list = hit_list->hsplist_array[0];

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
    seq_src = BlastSeqSrcFree(seq_src);
}

BOOST_AUTO_TEST_CASE(testBLASTPTraceBack) {
     const int k_num_hsps_start = 12;
     const int k_num_hsps_end = 10;
     
     CSeq_id qid("gi|42734333");
     CSeq_id sid("gi|30176631");
     
     auto_ptr<SSeqLoc> qsl(
         CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_unknown));
     auto_ptr<SSeqLoc> ssl(
         CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_unknown));

     CBlastProteinOptionsHandle opts_handle;
     CBl2Seq blaster(*qsl, *ssl, opts_handle);

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
                         opts_handle.GetOptions().GetProgramType());
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

     const int query_offset[k_num_hsps_start] = { 0, 3864, 3254, 1828, 2189, 795, 607, 1780, 1363, 2751, 3599, 242};
     const int query_end[k_num_hsps_start] = { 307, 4287, 3556, 2058, 2269, 914, 741, 1821, 1451, 2810, 3631, 285};
     const int subject_offset[k_num_hsps_start] = { 1, 2723, 2267, 1028, 1292, 634, 501, 925, 1195, 1795, 477, 1233};
     const int subject_end[k_num_hsps_start] = { 321, 3171, 2537, 1243, 1371, 749, 618, 966, 1286, 1869, 509, 1276};
     const int score[k_num_hsps_start] = { 370, 319, 139, 120, 89, 84, 75, 70, 69, 60, 47, 43};
     const int query_gapped_start[k_num_hsps_start] = { 47, 4181, 3286, 2034, 2228, 871, 632, 1798, 1383, 2759, 3606, 259};
     const int subject_gapped_start[k_num_hsps_start] = { 48, 3073, 2299, 1219, 1330, 709, 526, 943, 1215, 1803, 484, 1250};

    for (int index=0; index<k_num_hsps_start; index++)
    {
         hsp_list->hsp_array[index] = (BlastHSP*) calloc(1, sizeof(BlastHSP));
         hsp_list->hsp_array[index]->query.offset =query_offset[index];
         hsp_list->hsp_array[index]->query.end =query_end[index];
         hsp_list->hsp_array[index]->subject.offset =subject_offset[index];
         hsp_list->hsp_array[index]->subject.end =subject_end[index];
         hsp_list->hsp_array[index]->score =score[index];
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

    const int query_offset_final[k_num_hsps_end] = { 0, 3864, 3254, 1780, 2189, 607, 1363, 2751, 3599, 242};
    const int query_end_final[k_num_hsps_end] = { 307, 4287, 3556, 2058, 2599, 914, 1451, 2810, 3631, 285};
    const int subject_offset_final[k_num_hsps_end] = { 1, 2723, 2267, 925, 1292, 501, 1195, 1795, 477, 1233};
    const int subject_end_final[k_num_hsps_end] = { 321, 3171, 2537, 1243, 1704, 749, 1286, 1869, 509, 1276};
    const int score_final[k_num_hsps_end] = { 367, 319, 139, 131, 122, 104, 69, 60, 47, 43};
    const int query_gapped_start_final[k_num_hsps_end] = { 47, 4181, 3286, 2034, 2228, 871, 1383, 2759, 3606, 259};
    const int subject_gapped_start_final[k_num_hsps_end] = { 48, 3073, 2299, 1219, 1330, 709, 1215, 1803, 484, 1250};
    const int num_ident_final[k_num_hsps_end] = { 100, 122, 70, 61, 92, 54, 22, 18, 11, 9};

    BlastHitList* hit_list = results->hitlist_array[0];
    hsp_list = hit_list->hsplist_array[0];

    // One hsp is dropped when the function runs.
    BOOST_REQUIRE_EQUAL(k_num_hsps_end, hsp_list->hspcnt);
    for (int index=0; index<k_num_hsps_end; index++)
    {
         BlastHSP* tmp_hsp = hsp_list->hsp_array[index];
         BOOST_REQUIRE_EQUAL(query_offset_final[index], tmp_hsp->query.offset);
         BOOST_REQUIRE_EQUAL(query_end_final[index], tmp_hsp->query.end);
         BOOST_REQUIRE_EQUAL(subject_offset_final[index], tmp_hsp->subject.offset);
         BOOST_REQUIRE_EQUAL(subject_end_final[index], tmp_hsp->subject.end);
         BOOST_REQUIRE_EQUAL(score_final[index], tmp_hsp->score);
         BOOST_REQUIRE_EQUAL(query_gapped_start_final[index], tmp_hsp->query.gapped_start);
         BOOST_REQUIRE_EQUAL(subject_gapped_start_final[index], tmp_hsp->subject.gapped_start);
         BOOST_REQUIRE_EQUAL(num_ident_final[index], tmp_hsp->num_ident);
    }

    results = Blast_HSPResultsFree(results);
    seq_src = BlastSeqSrcFree(seq_src);
}

BOOST_AUTO_TEST_CASE(testTBLASTNTraceBack) {
     const int k_num_hsps_start = 16;
     const int k_num_hsps_end = 11;
     
     CSeq_id qid("gi|42734333");
     CSeq_id sid("gi|27902043");
     
     auto_ptr<SSeqLoc> qsl(
         CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_unknown));
     auto_ptr<SSeqLoc> ssl(
         CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

     CTBlastnOptionsHandle opts_handle;
     opts_handle.SetOptions().SetCompositionBasedStats(eNoCompositionBasedStats);
     opts_handle.SetOptions().SetFilterString("L");

     CBl2Seq blaster(*qsl, *ssl, opts_handle);

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
                         opts_handle.GetOptions().GetProgramType());
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

     const int query_offset[k_num_hsps_start] = { 149, 606, 656, 313, 221, 701, 57, 
           472, 0, 532, 404, 279, 125, 32, 371, 913};
     const int query_end[k_num_hsps_start] = { 189, 662, 705, 377, 281, 747, 138, 
           533, 33, 575, 472, 312, 150, 59, 405, 941};
     const int subject_offset[k_num_hsps_start] = { 58604, 59751, 59831, 58974, 
           58732, 59910, 58411, 59474, 58102, 59566, 59363, 58890, 58552, 58165, 59129, 9193};
     const int subject_end[k_num_hsps_start] = { 58644, 59807, 59880, 59038, 58792, 
           59956, 58489, 59535, 58135, 59609, 59432, 58923, 58577, 58192, 59163, 9221};
     const int score[k_num_hsps_start] = { 253, 237, 214, 193, 183, 178, 168, 165, 162, 
           149, 125, 120, 120, 113, 100, 55};
     const int subject_frame[k_num_hsps_start] = { -3, -2, -2, -2, -3, -2, -1, -3, -2, 
           -1, -2, -2, -3, -3, -3, 3};
     const int query_gapped_start[k_num_hsps_start] = { 173, 611, 662, 319, 254, 719, 72, 
           491, 11, 554, 438, 286, 131, 39, 379, 929};
     const int subject_gapped_start[k_num_hsps_start] = { 58629, 59756, 59837, 58980, 58765, 
           59928, 58426, 59493, 58113, 59588, 59399, 58897, 58558, 58172, 59137, 9209};

    for (int index=0; index<k_num_hsps_start; index++)
    {
         hsp_list->hsp_array[index] = (BlastHSP*) calloc(1, sizeof(BlastHSP));
         hsp_list->hsp_array[index]->query.offset =query_offset[index];
         hsp_list->hsp_array[index]->query.end =query_end[index];
         hsp_list->hsp_array[index]->subject.offset =subject_offset[index];
         hsp_list->hsp_array[index]->subject.end =subject_end[index];
         hsp_list->hsp_array[index]->score =score[index];
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

    const int query_offset_final[k_num_hsps_end] = {606, 125, 279, 57, 472, 0, 532, 
        428, 32, 371, 913};
    const int query_end_final[k_num_hsps_end] = {747, 281, 377, 138, 533, 33, 575, 
        472, 59, 405, 941};
    const int subject_offset_final[k_num_hsps_end] = {59751, 58552, 58890, 58411, 59474, 
        58102, 59566, 59389, 58165, 59129, 9193};
    const int subject_end_final[k_num_hsps_end] = {59956, 58792, 59038, 58489, 59535, 
        58135, 59609, 59432, 58192, 59163, 9221};
    const int score_final[k_num_hsps_end] = {525, 465, 250, 167, 165, 162, 149, 
        123, 113, 100, 55};
    const int subject_frame_final[k_num_hsps_end] = {-2, -3, -2, -1, -3, -2, 
        -1, -2, -3, -3, 3};
    const int query_gapped_start_final[k_num_hsps_end] = {611, 173, 319, 72, 491, 
        11, 554, 438, 39, 379, 929};
    const int subject_gapped_start_final[k_num_hsps_end] = {59756, 58629, 
        58980, 58426, 59493, 58113, 59588, 59399, 58172, 59137, 9209};
    const int num_ident_final[k_num_hsps_end] = {116, 105, 54, 44, 27, 31, 29, 
        25, 22, 21, 12};
    const int nums[k_num_hsps_end] = {1, 1, 1, 1, 2, 2, 2, 1, 2, 1, 1};

    BlastHitList* hit_list = results->hitlist_array[0];
    hsp_list = hit_list->hsplist_array[0];

    BOOST_REQUIRE_EQUAL(k_num_hsps_end, hsp_list->hspcnt);
    for (int index=0; index<k_num_hsps_end; index++)
    {
         BlastHSP* tmp_hsp = hsp_list->hsp_array[index];

         BOOST_REQUIRE_EQUAL(query_offset_final[index], tmp_hsp->query.offset);
         BOOST_REQUIRE_EQUAL(query_end_final[index], tmp_hsp->query.end);
         BOOST_REQUIRE_EQUAL(subject_offset_final[index], tmp_hsp->subject.offset);
         BOOST_REQUIRE_EQUAL(subject_end_final[index], tmp_hsp->subject.end);
         BOOST_REQUIRE_EQUAL(score_final[index], tmp_hsp->score);
         BOOST_REQUIRE_EQUAL(subject_frame_final[index], (int) tmp_hsp->subject.frame);
         BOOST_REQUIRE_EQUAL(query_gapped_start_final[index], tmp_hsp->query.gapped_start);
         BOOST_REQUIRE_EQUAL(subject_gapped_start_final[index], tmp_hsp->subject.gapped_start);
         BOOST_REQUIRE_EQUAL(num_ident_final[index], tmp_hsp->num_ident);
	 BOOST_REQUIRE_EQUAL(nums[index], tmp_hsp->num);
    }

    results = Blast_HSPResultsFree(results);
    seq_src = BlastSeqSrcFree(seq_src);
}

BOOST_AUTO_TEST_CASE(testNoHSPEvalueCutoffBeforeLink) {
     const EProgram kProgram = eTblastn;
     const EBlastProgramType kProgramType = eBlastTypeTblastn;
     const int kNumHsps = 3;
     const int q_offsets[kNumHsps] = { 1, 144, 203 };
     const int q_ends[kNumHsps] = { 151, 191, 226 };
     const int q_gapped_starts[kNumHsps] = { 23, 153, 209 };
     const int s_offsets[kNumHsps] = { 501, 655, 736 };
     const int s_ends[kNumHsps] = { 648, 702, 756 };
     const int s_gapped_starts[kNumHsps] = { 523, 664, 742 };
     const int s_frames[kNumHsps] = { 3, 1, 3 };
     const int scores[kNumHsps] = { 211, 91, 52 };
     const Int8 kSearchSp = 20763230804LL;
     const string kDbName("data/nt.41646578");
     
     CRef<CSeq_id> qid(new CSeq_id("gi|129295"));
     
     CRef<CBlastSearchQuery> Q1 = CTestObjMgr::Instance()
         .CreateBlastSearchQuery(*qid, eNa_strand_unknown);
     
     CBlastQueryVector query;
     query.AddQuery(Q1);
     
     CRef<CSeqDB> seqdb(new CSeqDB(kDbName, CSeqDB::eNucleotide));
     CBlastSeqSrc seq_src = SeqDbBlastSeqSrcInit(seqdb);
     CRef<blast::IBlastSeqInfoSrc> seq_info_src(new blast::CSeqDbSeqInfoSrc(seqdb));
     
     BlastHSPList* hsp_list = Blast_HSPListNew(kNumHsps);
     for (int index = 0; index < kNumHsps; ++index) {
        BlastHSP* hsp = hsp_list->hsp_array[index] = Blast_HSPNew();
        hsp->score = scores[index];
        hsp->query.offset = q_offsets[index];
        hsp->query.end = q_ends[index];
        hsp->query.gapped_start = q_gapped_starts[index];
        hsp->subject.offset = s_offsets[index];
        hsp->subject.end = s_ends[index];
        hsp->subject.gapped_start = s_gapped_starts[index];
        hsp->subject.frame = s_frames[index];
     }
     hsp_list->hspcnt = kNumHsps;

     BlastExtensionOptions* ext_options = NULL;
     BlastExtensionOptionsNew(kProgramType, &ext_options, true);

     BlastScoringOptions* scoring_options = NULL;
     BlastScoringOptionsNew(kProgramType, &scoring_options);

     BlastHitSavingOptions* hit_options = NULL;
     BlastHitSavingOptionsNew(kProgramType, &hit_options,
                              scoring_options->gapped_calculation);

	BlastHSPWriterInfo * writer_info = BlastHSPCollectorInfoNew(
	            BlastHSPCollectorParamsNew(
			hit_options, ext_options->compositionBasedStats,
            scoring_options->gapped_calculation));

	BlastHSPWriter* writer = BlastHSPWriterNew(&writer_info, NULL);
    BOOST_REQUIRE(writer_info == NULL);
    BlastHSPStream* hsp_stream = BlastHSPStreamNew(
			kProgramType, ext_options, FALSE, 1, writer);

     hit_options = BlastHitSavingOptionsFree(hit_options);
     scoring_options = BlastScoringOptionsFree(scoring_options);
     ext_options = BlastExtensionOptionsFree(ext_options);
     // needed after tie-breaking algorithm for scores was changed in
     // ScoreCompareHSP (blast_hits.c, revision 1.139)
     Blast_HSPListSortByScore(hsp_list); 
     BlastHSPStreamWrite(hsp_stream, &hsp_list);
     
     // Run traceback on this HSP list, without producing a Seq-align.
     
     CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query));
     
     CRef<CBlastOptionsHandle>
         cboh(CBlastOptionsFactory::Create(kProgram));
     
     CRef<CBlastOptions> opts
         (const_cast<CBlastOptions*>(& cboh->GetOptions()));
     
     cboh->SetEffectiveSearchSpace(kSearchSp);
     
     CRef< CStructWrapper<BlastHSPStream> > hsps
         (WrapStruct(hsp_stream, BlastHSPStreamFree));
     
     CBlastTracebackSearch search(qf, opts, seq_src.Get(), seq_info_src, hsps);

     CSearchResultSet crs = *search.Run();
     
     BOOST_REQUIRE_EQUAL((int) crs.GetNumResults(), 1);
     
     BOOST_REQUIRE_EQUAL(kNumHsps, (int)crs[0].GetSeqAlign()->Size());
}


BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: traceback-cppunit.cpp,v $
* Revision 1.76  2009/03/13 19:25:56  maning
* Previous commit messed up.  Roll back again.
*
* Revision 1.74  2009/03/13 18:45:15  maning
* Roll back to previous version.
*
* Revision 1.72  2008/12/16 21:24:10  madden
* Correct offsets
*
* Revision 1.71  2008/10/31 22:05:27  avagyanv
* Set old default values in options to avoid diffs in unit tests
*
* Revision 1.70  2008/10/27 17:00:13  camacho
* Fix include paths to deprecated headers
*
* Revision 1.69  2008/04/15 13:50:29  madden
* Update tests for svn 124499
*
* Revision 1.68  2008/02/13 21:39:12  camacho
* Re-enable choice to sort by score to meet pre-condition of composition-based
* statistics code.
*
* Revision 1.67  2007/10/22 19:16:10  madden
* BlastExtensionOptionsNew has Boolean gapped arg
*
* Revision 1.66  2007/07/27 18:04:34  papadopo
* change signature of HSPListCollector_Init
*
* Revision 1.65  2007/07/25 12:41:39  madden
* Accomodates changes to blastn type defaults
*
* Revision 1.64  2007/04/16 19:38:26  camacho
* Update code following CSearchResultSet changes
*
* Revision 1.63  2007/04/10 18:24:36  madden
* Remove discontinuous seq-aligns
*
* Revision 1.62  2007/03/22 14:34:44  camacho
* + support for auto-detection of genetic codes
*
* Revision 1.61  2007/03/20 14:54:02  camacho
* changes related to addition of multiple genetic code specification
*
* Revision 1.60  2006/10/16 19:33:22  madden
* Call in BlastHitSavingOptionsFree testNoHSPEvalueCutoffBeforeLink
*
* Revision 1.59  2006/10/10 19:47:01  bealer
* - Remove CDbBlast dependencies in favor of new traceback class.
*
* Revision 1.58  2006/06/29 16:25:24  camacho
* Changed BlastHitSavingOptionsNew signature
*
* Revision 1.57  2006/06/05 13:34:05  madden
* Changes to remove [GS]etMatrixPath and use callback instead
*
* Revision 1.56  2006/03/21 21:01:07  camacho
* + interruptible api support
*
* Revision 1.55  2006/02/15 18:41:11  madden
* Made changes to CBlastTraceBackTest::testBLASTNTraceBack to reflect
* small changes to the starting point due to changes to the
* BLAST_CheckStartForGappedAlignment routine.
* (from Mike Gertz).
*
* Revision 1.54  2005/12/16 20:51:50  camacho
* Diffuse the use of CSearchMessage, TQueryMessages, and TSearchMessages
*
* Revision 1.53  2005/10/14 13:47:32  camacho
* Fixes to pacify icc compiler
*
* Revision 1.52  2005/08/29 15:00:45  camacho
* Update calls to BLAST_MainSetUp to reflect changes in the signature
*
* Revision 1.51  2005/07/19 15:56:39  bealer
* - Undo incorrect Seq-id changes.
*
* Revision 1.50  2005/07/19 13:49:12  madden
* Fixes for dust change
*
* Revision 1.49  2005/07/18 19:17:51  bealer
* - Fix accessions.
*
* Revision 1.48  2005/07/18 17:04:43  bealer
* - Change expired GIs to (hopefully) longer lasting unversioned accessions.
*
* Revision 1.47  2005/06/09 20:37:06  camacho
* Use new private header blast_objmgr_priv.hpp
*
* Revision 1.46  2005/05/25 13:47:34  camacho
* Use CBLAST_SequenceBlk instead of BLAST_SequenceBlk*
*
* Revision 1.45  2005/05/24 20:05:17  camacho
* Changed signature of SetupQueries and SetupQueryInfo
*
* Revision 1.44  2005/05/23 15:53:19  dondosha
* Special case for preliminary hitlist size in RPS BLAST - hence no need for 2 extra parameters in SBlastHitsParametersNew
*
* Revision 1.43  2005/05/17 16:05:23  madden
* Fixes for BLOSUM62 matrix change
*
* Revision 1.42  2005/05/16 12:29:15  madden
* use SBlastHitsParameters in Blast_HSPListCollectorInit and Blast_HSPListCollectorInit[MT]
*
* Revision 1.41  2005/04/27 20:09:38  dondosha
* Extra argument has been added to BLAST_ComputeTraceback for PHI BLAST, pass NULL
*
* Revision 1.40  2005/04/18 14:01:55  camacho
* Updates following BlastSeqSrc reorganization
*
* Revision 1.39  2005/04/06 21:26:37  dondosha
* GapEditBlock structure and redundant fields in BlastHSP have been removed
*
* Revision 1.38  2005/03/31 13:45:58  camacho
* BLAST options API clean-up
*
* Revision 1.37  2005/03/14 19:44:59  madden
* Adjustments for recent dust change
*
* Revision 1.36  2005/03/04 17:20:45  bealer
* - Command line option support.
*
* Revision 1.35  2005/01/27 20:09:54  madden
* Adjustments for dust fixes from Richa
*
* Revision 1.34  2005/01/19 19:07:17  coulouri
* do not cast pointers to smaller integer types
*
* Revision 1.33  2005/01/18 14:55:05  camacho
* Changes for taking new tie-breakers for score comparison into account
*
* Revision 1.32  2005/01/06 15:43:25  camacho
* Make use of modified signature to blast::SetupQueries
*
* Revision 1.31  2004/12/21 17:26:11  dondosha
* BLAST_ComputeTraceback has a new RPSInfo argument; pass it as NULL for non-RPS searches
*
* Revision 1.30  2004/12/08 16:43:55  dondosha
* Removed dead code
*
* Revision 1.29  2004/11/30 17:05:35  dondosha
* Added test to check that HSPs are not removed based on e-value cutoff before linking
*
* Revision 1.28  2004/11/18 21:35:43  dondosha
* Added testHSPUpdateWithTraceback
*
* Revision 1.27  2004/11/17 21:02:01  camacho
* Add error checking to BlastSeqSrc initialization
*
* Revision 1.26  2004/10/19 16:38:14  dondosha
* Changed order of results due to sorting of HSPs by score in BLAST_LinkHsps
*
* Revision 1.25  2004/09/29 17:20:08  papadopo
* shuffle the order of expected HSPs to account for changes in HSP linking
*
* Revision 1.24  2004/09/21 13:54:28  dondosha
* Adjusted results due to fix in the engine that sorts HSPs in HSP lists by e-value
*
* Revision 1.23  2004/07/19 15:04:22  dondosha
* Renamed multiseq_src to seqsrc_multiseq, seqdb_src to seqsrc_seqdb
*
* Revision 1.22  2004/07/06 15:58:45  dondosha
* Use EBlastProgramType enumeration type for program when calling C functions
*
* Revision 1.21  2004/06/28 13:43:51  madden
* Use NULL for unused filter_slp in BLAST_MainSetUp
*
* Revision 1.20  2004/06/24 16:10:26  madden
* Add test testBLASTNTraceBackLargeXDrop that core-dumped before blast_gapalign.c fix from Jason Papadopoulos on 6/24/04
*
* Revision 1.19  2004/06/22 16:46:19  camacho
* Changed the blast_type_* definitions for the EBlastProgramType enumeration.
*
* Revision 1.18  2004/06/08 15:24:34  dondosha
* Use BlastHSPStream interface
*
* Revision 1.17  2004/05/12 12:20:34  madden
* Add (NULL) psi_options to call to BLAST_ComputeTraceback
*
* Revision 1.16  2004/05/07 15:44:42  papadopo
* fill in and use BlastScoringParameters instead of BlastScoringOptions
*
* Revision 1.15  2004/05/05 15:29:39  dondosha
* Renamed functions in blast_hits.h accordance with new convention Blast_[StructName][Task]
*
* Revision 1.14  2004/04/30 16:54:05  dondosha
* Changed a number of function names to have the same conventional Blast_ prefix
*
* Revision 1.13  2004/04/19 13:10:47  madden
* Remove duplicate lines/checks
*
* Revision 1.12  2004/03/23 22:07:01  camacho
* Fix memory leaks
*
* Revision 1.11  2004/03/23 16:10:34  camacho
* Minor changes to CTestObjMgr
*
* Revision 1.10  2004/03/18 15:15:25  dondosha
* Changed duplicate variable names due to compiler warnings
*
* Revision 1.9  2004/03/15 20:02:38  dondosha
* Use BLAST_GapAlignSetUp function to do traceback specific setup; database and two sequences traceback engines merged; corrected order of arguments in BOOST_REQUIRE_EQUAL calls
*
* Revision 1.8  2004/03/11 21:17:16  camacho
* Fix calls to BlastHitSavingParametersNew
*
* Revision 1.7  2004/03/09 18:58:56  dondosha
* Added extension parameters argument to BlastHitSavingParametersNew calls
*
* Revision 1.6  2004/03/01 14:14:50  madden
* Add check for number of identitical letters in final alignment
*
* Revision 1.5  2004/02/27 21:26:56  madden
* Cleanup testBLASTNTraceBack
*
* Revision 1.4  2004/02/27 21:22:25  madden
* Add tblastn test: testTBLASTNTraceBack
*
* Revision 1.3  2004/02/27 20:34:34  madden
* Add setUp and tearDown routines, use tearDown for deallocation
*
* Revision 1.2  2004/02/27 20:14:24  madden
* Add protein-protein test: testBLASTPTraceBack
*
* Revision 1.1  2004/02/27 19:45:35  madden
* Unit test for traceback (mostly BlastHSPListGetTraceback)
*
*
*
* ===========================================================================
*/
