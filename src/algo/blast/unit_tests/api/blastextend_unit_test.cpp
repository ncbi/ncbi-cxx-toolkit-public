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
* Author: Ilya Dondoshansky
*
* File Description:
*   Unit test module to test the nucleotide gapped alignment part of BLAST
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <corelib/ncbitime.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <algo/blast/api/blast_nucl_options.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_gapalign.h>
#include <blast_objmgr_priv.hpp>
#ifdef NCBI_OS_IRIX
#include <stdlib.h>
#else
#include <cstdlib>
#endif

#include "test_objmgr.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

extern "C" int x_score_compare_hsps(const void* v1, const void* v2)
{
    BlastHSP* h1,* h2;
    
    h1 = *((BlastHSP**) v1);
    h2 = *((BlastHSP**) v2);
    
    if (h1->score > h2->score)
        return -1;
    else if (h1->score < h2->score)
        return 1;
    return 0;
}

struct CBlastExtendTestFixture
{
    CBlastQueryInfo m_iclsQueryInfo;
    CBLAST_SequenceBlk m_iclsQueryBlk;

    BlastScoreBlk* m_ipScoreBlk;
    BlastScoringParameters* m_ipScoreParams;
    BlastExtensionParameters* m_ipExtParams;
    BlastHitSavingParameters* m_ipHitParams;
    BlastGapAlignStruct* m_ipGapAlign;
    BlastInitHitList* m_ipInitHitlist;

    BlastScoringOptions*    m_ScoringOpts;
    BlastExtensionOptions*  m_ExtnOpts;
    BlastHitSavingOptions*  m_HitSavingOpts;

    CBlastExtendTestFixture() {
        m_ScoringOpts = NULL;
        m_ExtnOpts = NULL;
        m_HitSavingOpts = NULL;
        m_ipScoreBlk = NULL;
        m_ipInitHitlist = NULL;
        m_ipGapAlign = NULL;
        m_ipScoreParams = NULL;
        m_ipHitParams = NULL;
        m_ipExtParams = NULL;
    }

    ~CBlastExtendTestFixture()
    {
        m_ipScoreBlk = BlastScoreBlkFree(m_ipScoreBlk);
        m_ipInitHitlist = BLAST_InitHitListFree(m_ipInitHitlist);
        m_ipGapAlign = BLAST_GapAlignStructFree(m_ipGapAlign);
        m_ipHitParams = BlastHitSavingParametersFree(m_ipHitParams);
        sfree(m_ipScoreParams);
        sfree(m_ipHitParams);
        sfree(m_ipExtParams);

        BlastScoringOptionsFree(m_ScoringOpts);
        BlastExtensionOptionsFree(m_ExtnOpts);
        BlastHitSavingOptionsFree(m_HitSavingOpts);
    }

    void setupHitList()
    {
        const int num_hsps = 8;
        const int q_offsets[num_hsps] = 
            {8799, 1358, 14042, 27664, 5143, 27737, 5231, 3212 };
        const int s_offsets[num_hsps] = 
            { 2728, 2736, 2784, 2784, 2792, 2856, 2888, 3640 };
        const int q_starts[num_hsps] = 
            { 8794, 1355, 14015, 27637, 5131, 27732, 5226, 3201 };
        const int s_starts[num_hsps] = 
            { 2723, 2733, 2757, 2757, 2780, 2851, 2883, 3629 };
        const int lengths[num_hsps] = { 174, 18, 141, 92, 38, 37, 28, 20 };
        const int scores[num_hsps] = { 146, 18, 93, 40, 34, 21, 24, 16 };

        m_ipInitHitlist = BLAST_InitHitListNew();       
        BlastUngappedData* ungapped_data;
        Int4 index;

        for (index = 0; index < num_hsps; ++index) {
            ungapped_data = 
                (BlastUngappedData*) calloc(1, sizeof(BlastUngappedData));
            ungapped_data->q_start = q_starts[index];
            ungapped_data->s_start = s_starts[index];
            ungapped_data->length = lengths[index];
            ungapped_data->score = scores[index];
            BLAST_SaveInitialHit(m_ipInitHitlist, q_offsets[index], 
                                 s_offsets[index], ungapped_data);
        }
    }

    void setupGreedyHitList()
    {
        const int num_hsps = 14;
        const int q_offsets[num_hsps] = 
            { 8799, 1358, 8831, 14042, 27664, 5143, 8863, 8903, 8927, 14114, 
              27737, 8943, 5231, 3212 };
        const int s_offsets[num_hsps] = 
            { 2728, 2736, 2760, 2784, 2784, 2792, 2792, 2832, 2856, 2856, 
              2856, 2872, 2888, 3640 };

        m_ipInitHitlist = BLAST_InitHitListNew();       
        
        Int4 index;
        for (index = 0; index < num_hsps; ++index) {
            BLAST_SaveInitialHit(m_ipInitHitlist, q_offsets[index], 
                                 s_offsets[index], NULL);
        }
    }

    void 
    fillEffectiveLengths(EBlastProgramType program_type,
                         const BlastScoringOptions* score_options,
                         Int8 db_length, Int4 db_num_seq) {
        BlastEffectiveLengthsOptions* eff_len_options = NULL;
        BlastEffectiveLengthsOptionsNew(&eff_len_options);
        BlastEffectiveLengthsParameters* eff_len_params = NULL;
        BlastEffectiveLengthsParametersNew(eff_len_options, db_length, 
                                           db_num_seq, &eff_len_params);
        BLAST_CalcEffLengths(program_type, score_options, eff_len_params, 
                             m_ipScoreBlk, m_iclsQueryInfo, NULL);
        BlastEffectiveLengthsParametersFree(eff_len_params);
        BlastEffectiveLengthsOptionsFree(eff_len_options);
    }

    void setupStructures(Uint4 subject_length, bool greedy) 
    {
        Int2 status;

        const EBlastProgramType kCoreProgramType = eBlastTypeBlastn;

        status = BlastScoringOptionsNew(kCoreProgramType, &m_ScoringOpts);
        BOOST_REQUIRE(status == 0);
        
        m_ipScoreBlk = BlastScoreBlkNew(BLASTNA_SEQ_CODE, 2);
        status = Blast_ScoreBlkMatrixInit(kCoreProgramType, m_ScoringOpts,
                     m_ipScoreBlk, &BlastFindMatrixPath);

        BOOST_REQUIRE(status == 0);
        Blast_Message* message = NULL;
        status = Blast_ScoreBlkKbpUngappedCalc(
                     kCoreProgramType, m_ipScoreBlk, 
                     m_iclsQueryBlk->sequence, m_iclsQueryInfo,
                     &message);
        message = Blast_MessageFree(message);

        BOOST_REQUIRE(status == 0);
        status = Blast_ScoreBlkKbpGappedCalc(m_ipScoreBlk, m_ScoringOpts,
                     kCoreProgramType, m_iclsQueryInfo, NULL);
    
        BOOST_REQUIRE(status == 0);
    
        m_ipScoreBlk->kbp = m_ipScoreBlk->kbp_std;
        m_ipScoreBlk->kbp_gap = m_ipScoreBlk->kbp_gap_std;

        fillEffectiveLengths(kCoreProgramType, m_ScoringOpts, 
                             subject_length, 1);

        BlastScoringParametersNew(m_ScoringOpts,
                                  m_ipScoreBlk, &m_ipScoreParams);

        status = BlastExtensionOptionsNew(kCoreProgramType, &m_ExtnOpts, true);
        if (greedy)
            m_ExtnOpts->ePrelimGapExt = eGreedyScoreOnly;

        BOOST_REQUIRE(status == 0);

        BlastExtensionParametersNew(kCoreProgramType, 
                                    m_ExtnOpts, m_ipScoreBlk, 
                                    m_iclsQueryInfo, &m_ipExtParams);

        status = BlastHitSavingOptionsNew(kCoreProgramType, &m_HitSavingOpts,
                                          m_ScoringOpts->gapped_calculation);
        BOOST_REQUIRE(status == 0);
        
        BlastHitSavingParametersNew(kCoreProgramType, m_HitSavingOpts,
				    m_ipScoreBlk, m_iclsQueryInfo, 0, &m_ipHitParams);
        
        status = BLAST_GapAlignStructNew(m_ipScoreParams, m_ipExtParams, 
                                         subject_length, m_ipScoreBlk, &m_ipGapAlign);
        BOOST_REQUIRE(status == 0);
    }
};

BOOST_FIXTURE_TEST_SUITE(BlastExtend, CBlastExtendTestFixture)

BOOST_AUTO_TEST_CASE(testGapAlignment) {
    const int num_hsps = 7;
    const int query_starts[num_hsps] = 
        { 8794, 13982, 12612, 5131, 5226, 1355, 3201 };
    const int subject_starts[num_hsps] = 
        { 2723, 2723, 2733, 2780, 2883, 2733, 3629 };
    const int query_lengths[num_hsps] = { 174, 174, 182, 38, 28, 18, 20 };
    const int subject_lengths[num_hsps] = { 174, 175, 183, 38, 28, 18, 20 };
    BlastGappedStats* gapped_stats = NULL;

    CSeq_id qid("gi|2655203");
    pair<TSeqPos, TSeqPos> range(20000, 35000);
    auto_ptr<SSeqLoc> qsl(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, eNa_strand_both));
    CSeq_id sid("gi|2516238");
    auto_ptr<SSeqLoc> ssl(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    CBlastNucleotideOptionsHandle opts_handle;
    TSeqLocVector queries;
    TSeqLocVector subjects;
    queries.push_back(*qsl);
    subjects.push_back(*ssl);

    const CBlastOptions& kOpts = opts_handle.GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();
    TSearchMessages blast_msg;

    SetupQueryInfo(queries, prog, strand_opt, &m_iclsQueryInfo); 
    SetupQueries(queries, m_iclsQueryInfo, &m_iclsQueryBlk, 
                    prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_REQUIRE(m->empty());
    }
    
    Uint4 subject_length;
    vector<BLAST_SequenceBlk*> subject_blk_v;
    SetupSubjects(subjects, opts_handle.GetOptions().GetProgramType(), 
                    &subject_blk_v, &subject_length);

    setupStructures(subject_length, false);

    setupHitList();
    
    Blast_InitHitListSortByScore(m_ipInitHitlist);

    BlastHSPList* hsp_list = Blast_HSPListNew(0);
    gapped_stats = 
        (BlastGappedStats*) calloc(1, sizeof(BlastGappedStats));

    BLAST_GetGappedScore(opts_handle.GetOptions().GetProgramType(), 
                            m_iclsQueryBlk, m_iclsQueryInfo, subject_blk_v[0],
                            m_ipGapAlign, m_ipScoreParams, m_ipExtParams, 
                            m_ipHitParams, m_ipInitHitlist, &hsp_list, 
                            gapped_stats, NULL);

    BlastSequenceBlkFree(subject_blk_v[0]);
    BOOST_REQUIRE_EQUAL(num_hsps, hsp_list->hspcnt);

    BOOST_REQUIRE_EQUAL(num_hsps, gapped_stats->extensions);

    sfree(gapped_stats);

    qsort(hsp_list->hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*), 
            x_score_compare_hsps);
    Int4 index;
    for (index = 0; index < num_hsps; ++index) {
        BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[index]->query.offset, 
                                query_starts[index]);
        BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[index]->subject.offset, 
                                subject_starts[index]);
        BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[index]->query.end - 
                                hsp_list->hsp_array[index]->query.offset, 
                                query_lengths[index]);
        BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[index]->subject.end - 
                                hsp_list->hsp_array[index]->subject.offset, 
                                subject_lengths[index]);
    }

    Blast_HSPListFree(hsp_list);
}

BOOST_AUTO_TEST_CASE(testGreedyAlignment) {
    const int num_hsps = 7;
    const int query_starts[num_hsps] = 
        { 8794, 13982, 12612, 5131, 5226, 1355, 3201 };
    const int subject_starts[num_hsps] = 
        { 2723, 2723, 2733, 2780, 2883, 2733, 3629 };
    const int query_lengths[num_hsps] = 
        { 174, 174, 182, 38, 28, 18, 20 };
    const int subject_lengths[num_hsps] = 
        { 174, 175, 183, 38, 28, 18, 20 };
    BlastGappedStats* gapped_stats = NULL;


    CSeq_id qid("gi|2655203");
    pair<TSeqPos, TSeqPos> range(20000, 35000);
    auto_ptr<SSeqLoc> qsl(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, eNa_strand_both));
    CSeq_id sid("gi|2516238");
    auto_ptr<SSeqLoc> ssl(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    CBlastNucleotideOptionsHandle opts_handle;

    TSeqLocVector queries;
    TSeqLocVector subjects;
    queries.push_back(*qsl);
    subjects.push_back(*ssl);

    const CBlastOptions& kOpts = opts_handle.GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();
    TSearchMessages blast_msg;

    SetupQueryInfo(queries, prog, strand_opt, &m_iclsQueryInfo); 
    SetupQueries(queries, m_iclsQueryInfo, &m_iclsQueryBlk, 
                    prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_REQUIRE(m->empty());
    }
    
    Uint4 subject_length;
    vector<BLAST_SequenceBlk*> subject_blk_v;
    SetupSubjects(subjects, opts_handle.GetOptions().GetProgramType(), 
                    &subject_blk_v, &subject_length);

    setupStructures(subject_length, true);

    setupGreedyHitList();
    
    BlastHSPList* hsp_list = Blast_HSPListNew(0);
    gapped_stats = 
        (BlastGappedStats*) calloc(1, sizeof(BlastGappedStats));

    BLAST_GetGappedScore(opts_handle.GetOptions().GetProgramType(), 
                            m_iclsQueryBlk, m_iclsQueryInfo, subject_blk_v[0],
                            m_ipGapAlign, m_ipScoreParams, m_ipExtParams, 
                            m_ipHitParams, m_ipInitHitlist, &hsp_list, 
                            gapped_stats, NULL);

    BOOST_REQUIRE_EQUAL(num_hsps, hsp_list->hspcnt);

    // Now test that introduction of a percent identity and length cutoffs
    // does not influence the BLAST_MbGetGappedScore behavior.
    // Free the HSPList
    hsp_list = Blast_HSPListFree(hsp_list);
    // The initial seeds have been modified if they were on reverse strand,
    // so setup the initial hit list again.
    m_ipInitHitlist = BLAST_InitHitListFree(m_ipInitHitlist);
    setupGreedyHitList();
    
    // Set the percent identity and minimal length cutoffs
    m_ipHitParams->options->min_hit_length = 100;
    m_ipHitParams->options->percent_identity = 99;

    BLAST_GetGappedScore(opts_handle.GetOptions().GetProgramType(), 
                            m_iclsQueryBlk, m_iclsQueryInfo, subject_blk_v[0],
                            m_ipGapAlign, m_ipScoreParams, m_ipExtParams, 
                            m_ipHitParams, m_ipInitHitlist, &hsp_list, 
                            gapped_stats, NULL);

    BOOST_REQUIRE_EQUAL(num_hsps, hsp_list->hspcnt);

    BlastSequenceBlkFree(subject_blk_v[0]);

    // Since gapped alignment function was called twice, the number of
    // extensions is double the real one.
    BOOST_REQUIRE_EQUAL(2*num_hsps, gapped_stats->extensions);

    sfree(gapped_stats);

    qsort(hsp_list->hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*), 
            x_score_compare_hsps);
    Int4 index;
    for (index = 0; index < num_hsps; ++index) {
        BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[index]->query.offset, 
                                query_starts[index]);
        BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[index]->subject.offset, 
                                subject_starts[index]);
        BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[index]->query.end -
                                hsp_list->hsp_array[index]->query.offset, 
                                query_lengths[index]);
        BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[index]->subject.end -
                                hsp_list->hsp_array[index]->subject.offset, 
                                subject_lengths[index]);
    }

    Blast_HSPListFree(hsp_list);
}

// Test for SB-666 fix
BOOST_AUTO_TEST_CASE(testGreedyAlignmentWithBadStart) {
    const int query_start = 2667;
    const int query_end = 2754;
    const int subject_start = 350;
    const int subject_end = 438;
    const int q_offset = 2754;
    const int s_offset = 438;
    BlastGappedStats* gapped_stats = NULL;
    BlastUngappedData ungapped;

    ungapped.q_start = 2671;
    ungapped.s_start = 355;
    ungapped.length = 167;
    ungapped.score = 42;

    CSeq_id qid("gi|156523973");
    auto_ptr<SSeqLoc> qsl(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    CSeq_id sid("gi|224514626");
    pair<TSeqPos, TSeqPos> range(1896999, 1897550);
    auto_ptr<SSeqLoc> ssl(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range, eNa_strand_both));

    CBlastNucleotideOptionsHandle opts_handle;

    TSeqLocVector queries;
    TSeqLocVector subjects;
    queries.push_back(*qsl);
    subjects.push_back(*ssl);

    const CBlastOptions& kOpts = opts_handle.GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();
    TSearchMessages blast_msg;

    SetupQueryInfo(queries, prog, strand_opt, &m_iclsQueryInfo); 
    SetupQueries(queries, m_iclsQueryInfo, &m_iclsQueryBlk, 
                    prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_REQUIRE(m->empty());
    }
    
    Uint4 subject_length;
    vector<BLAST_SequenceBlk*> subject_blk_v;
    SetupSubjects(subjects, opts_handle.GetOptions().GetProgramType(), 
                    &subject_blk_v, &subject_length);

    setupStructures(subject_length, true);

    // The following options must be patched to reproduce SB-666
    m_ipScoreParams->reward = 1;
    m_ipScoreParams->penalty = -2;
    m_ipScoreParams->gap_open = 0;
    m_ipScoreParams->gap_extend = 0;

    m_ipExtParams->gap_x_dropoff = 16;
    m_ipExtParams->gap_x_dropoff_final = 54;

    m_ipGapAlign = BLAST_GapAlignStructFree(m_ipGapAlign);

    BLAST_GapAlignStructNew(m_ipScoreParams, m_ipExtParams, 
                            subject_length, m_ipScoreBlk, &m_ipGapAlign);

    m_ipInitHitlist = BLAST_InitHitListNew();       

    BLAST_SaveInitialHit(m_ipInitHitlist, q_offset, s_offset, &ungapped);
    
    BlastHSPList* hsp_list = Blast_HSPListNew(0);
    gapped_stats = 
        (BlastGappedStats*) calloc(1, sizeof(BlastGappedStats));

    BLAST_GetGappedScore(opts_handle.GetOptions().GetProgramType(), 
                            m_iclsQueryBlk, m_iclsQueryInfo, subject_blk_v[0],
                            m_ipGapAlign, m_ipScoreParams, m_ipExtParams, 
                            m_ipHitParams, m_ipInitHitlist, &hsp_list, 
                            gapped_stats, NULL);

    m_ipInitHitlist->init_hsp_array[0].ungapped_data = NULL;

    BOOST_REQUIRE_EQUAL(1, hsp_list->hspcnt);

    BlastSequenceBlkFree(subject_blk_v[0]);

    sfree(gapped_stats);

    BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[0]->query.offset, query_start);
    BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[0]->subject.offset, subject_start);
    BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[0]->query.end, query_end);
    BOOST_REQUIRE_EQUAL(hsp_list->hsp_array[0]->subject.end, subject_end);

    // The following are required to fix SB-666
    BOOST_REQUIRE(m_ipGapAlign->greedy_query_seed_start >= query_start);
    BOOST_REQUIRE(m_ipGapAlign->greedy_query_seed_start <= query_end);
    BOOST_REQUIRE(m_ipGapAlign->greedy_subject_seed_start >= subject_start);
    BOOST_REQUIRE(m_ipGapAlign->greedy_subject_seed_start <= subject_end);

    Blast_HSPListFree(hsp_list);
}

BOOST_AUTO_TEST_CASE(testSmallMBSpaceValue) {
        const int kSize = 100;
        const int kDefaultSize = 1000000;
        SMBSpace* retval = MBSpaceNew(kSize);
        BOOST_REQUIRE(retval);
        BOOST_REQUIRE_EQUAL(kDefaultSize, retval->space_allocated);
        MBSpaceFree(retval);
}

BOOST_AUTO_TEST_CASE(testZeroMBSpaceValue) {
        const int kSize = 0;
        const int kDefaultSize = 1000000;
        SMBSpace* retval = MBSpaceNew(kSize);
        BOOST_REQUIRE(retval);
        BOOST_REQUIRE_EQUAL(kDefaultSize, retval->space_allocated);
        MBSpaceFree(retval);
}

BOOST_AUTO_TEST_CASE(testLargeMBSpaceValue) {
        const int kSize = 5000000;
        SMBSpace* retval = MBSpaceNew(kSize);
        BOOST_REQUIRE(retval);
        BOOST_REQUIRE_EQUAL(kSize, retval->space_allocated);
        MBSpaceFree(retval);
}

BOOST_AUTO_TEST_CASE(testInitHitListFreeWithNULLInput) {
        BlastInitHitList* input = NULL;
        BlastInitHitList* output = NULL;
        bool null_output = false;
        output = BLAST_InitHitListFree(input);
        if (output == NULL)
            null_output = true;
        BOOST_REQUIRE_EQUAL(true, null_output);
}

BOOST_AUTO_TEST_CASE(testBlastExtendWordFreeWithNULLInput) {
        Blast_ExtendWord* input = NULL;
        Blast_ExtendWord* output = NULL;
        bool null_output = false;
        output = BlastExtendWordFree(input);
        if (output == NULL)
            null_output = true;
        BOOST_REQUIRE_EQUAL(true, null_output);
}

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: blastextend-cppunit.cpp,v $
* Revision 1.55  2008/07/18 14:05:21  camacho
* Irix fixes
*
* Revision 1.54  2007/10/22 19:16:09  madden
* BlastExtensionOptionsNew has Boolean gapped arg
*
* Revision 1.53  2007/03/20 14:54:02  camacho
* changes related to addition of multiple genetic code specification
*
* Revision 1.52  2007/02/08 17:13:29  papadopo
* change enum value
*
* Revision 1.51  2006/11/29 17:26:16  bealer
* - HSP range support.
*
* Revision 1.50  2006/09/08 17:17:09  camacho
* Fix memory leaks
*
* Revision 1.49  2006/06/29 16:25:24  camacho
* Changed BlastHitSavingOptionsNew signature
*
* Revision 1.48  2006/06/05 13:34:05  madden
* Changes to remove [GS]etMatrixPath and use callback instead
*
* Revision 1.47  2006/05/18 16:32:03  papadopo
* change signature of BLAST_CalcEffLengths
*
* Revision 1.46  2006/04/20 19:35:05  madden
* Blast_ScoreBlkKbpUngappedCalc prototype change
*
* Revision 1.45  2006/01/23 16:53:44  papadopo
* replace BLAST_MbGetGappedScore
*
* Revision 1.44  2005/12/16 20:51:50  camacho
* Diffuse the use of CSearchMessage, TQueryMessages, and TSearchMessages
*
* Revision 1.43  2005/10/14 13:47:32  camacho
* Fixes to pacify icc compiler
*
* Revision 1.42  2005/08/15 16:13:08  dondosha
* Added new argument in call to Blast_ScoreBlkKbpGappedCalc
*
* Revision 1.41  2005/06/09 20:37:05  camacho
* Use new private header blast_objmgr_priv.hpp
*
* Revision 1.40  2005/05/24 20:05:17  camacho
* Changed signature of SetupQueries and SetupQueryInfo
*
* Revision 1.39  2005/04/11 14:04:46  dondosha
* Really do greedy alignment in testGreedyAlignment test, and check that it works with affine gap penalties - it failed because of premature perc. identity check
*
* Revision 1.38  2005/04/07 19:38:09  madden
* Add MBSpaceNew checks as well as NULL input checks on BLAST_InitHitListFree and BlastExtendWordFree
*
* Revision 1.37  2005/04/06 21:26:37  dondosha
* GapEditBlock structure and redundant fields in BlastHSP have been removed
*
* Revision 1.36  2005/03/31 13:45:58  camacho
* BLAST options API clean-up
*
* Revision 1.35  2005/03/29 15:03:30  papadopo
* fill in all search spaces for valid contexts (engine requires this now)
*
* Revision 1.34  2005/03/29 14:20:45  camacho
* Refactorings
*
* Revision 1.33  2005/03/04 17:20:44  bealer
* - Command line option support.
*
* Revision 1.32  2005/01/10 14:02:49  madden
* Removed calls to SetScanStep
*
* Revision 1.31  2005/01/06 15:43:25  camacho
* Make use of modified signature to blast::SetupQueries
*
* Revision 1.30  2004/12/09 15:25:11  dondosha
* BLAST_ScoreBlkFill changed to Blast_ScoreBlkUngappedCalc
*
* Revision 1.29  2004/12/02 16:50:13  bealer
* - Change multiple-arrays to array-of-struct in BlastQueryInfo
*
* Revision 1.28  2004/11/02 18:30:17  madden
* BlastHitSavingParametersNew no longer requires BlastExtensionParameters
*
* Revision 1.27  2004/10/19 16:39:46  dondosha
* Sort input initial hit list by score, as this order is expected in gapped alignment routines
*
* Revision 1.26  2004/10/14 17:13:56  madden
* New parameter in BlastHitSavingParametersNew
*
* Revision 1.25  2004/07/06 15:58:45  dondosha
* Use EBlastProgramType enumeration type for program when calling C functions
*
* Revision 1.24  2004/06/08 19:28:01  dondosha
* Removed unused argument in call to BLAST_GapAlignStructNew
*
* Revision 1.23  2004/05/17 15:44:02  dondosha
* Memory leak fixes
*
* Revision 1.22  2004/05/14 17:17:39  dondosha
* Check diagnostics information returned from BLAST engine
*
* Revision 1.21  2004/05/07 15:42:06  papadopo
* fill in and use BlastScoringParameters instead of BlastScoringOptions
*
* Revision 1.20  2004/04/21 17:34:14  madden
* Use cleaned up API for saving HSPs, HSPLists, HitLists
*
* Revision 1.19  2004/04/07 03:06:21  camacho
* Added blast_encoding.[hc], refactoring blast_stat.[hc]
*
* Revision 1.18  2004/03/26 21:41:48  dondosha
* Use const int instead of hard coded constants in array sizes
*
* Revision 1.17  2004/03/24 22:14:22  dondosha
* Fixed memory leaks
*
* Revision 1.16  2004/03/24 19:21:40  dondosha
* BLAST_InitHitListDestruct name changed to BLAST_InitHitListFree
*
* Revision 1.15  2004/03/23 16:10:34  camacho
* Minor changes to CTestObjMgr
*
* Revision 1.14  2004/03/15 20:00:56  dondosha
* SetupSubjects prototype changed to take just program instead of CBlastOptions*
*
* Revision 1.13  2004/03/11 21:17:16  camacho
* Fix calls to BlastHitSavingParametersNew
*
* Revision 1.12  2004/03/09 18:58:44  dondosha
* Added extension parameters argument to BlastHitSavingParametersNew calls
*
* Revision 1.11  2004/02/27 15:57:20  papadopo
* change initialization of ScoreBlk
*
* Revision 1.10  2004/02/20 23:20:36  camacho
* Remove undefs.h
*
* Revision 1.9  2004/02/20 21:47:18  camacho
* Rename score_compare_hsps as it collides with function in libncbitool
*
* Revision 1.8  2004/02/20 19:55:00  camacho
* Fix compare function for usage with qsort
*
* Revision 1.7  2004/02/18 00:35:50  dondosha
* Reinstated changes from revision 1.4 - they are valid no
*
* Revision 1.6  2004/02/17 21:52:17  dondosha
* Query info argument to calls to gapped alignment will have to be added just a little bit later
*
* Revision 1.5  2004/02/17 20:42:47  dondosha
* One data change in previous commit needs to wait a couple of hours longer for other relevant files
*
* Revision 1.4  2004/02/17 20:33:12  dondosha
* Use BOOST_REQUIRE_EQUAL; const int array sizes
*
* Revision 1.3  2004/01/30 23:22:21  dondosha
* Use getters for options structures because of the API change
*
* Revision 1.2  2004/01/09 21:58:28  dondosha
* Added a test for greedy alignment
*
* Revision 1.1  2004/01/08 23:20:54  dondosha
* Test for gapped extensions
*
*
* ===========================================================================
*/
