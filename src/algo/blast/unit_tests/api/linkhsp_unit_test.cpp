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
*   Unit test module to test the algorithms for linking HSPs
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <corelib/ncbitime.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/util/sequence.hpp>

#include "test_objmgr.hpp"

#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/link_hsps.h>
#include <algo/blast/api/blast_options.hpp>
#include <blast_objmgr_priv.hpp>
#include <algo/blast/api/seqsrc_seqdb.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

struct AllCutoffScores {
    Int4 x_drop_ungapped;
    Int4 x_drop_gapped;
    Int4 x_drop_final;
    Int4 gap_trigger;
    Int4 cutoff_score_ungapped;
    Int4 cutoff_score_final;
    Boolean do_sum_stats;
    Int4 cutoff_small_gap;
    Int4 cutoff_big_gap;
};

/// Sets up the query information structure without a real sequence. Used 
/// only for blastn test below, where query sequence is not available. 
static void 
s_SetupNuclQueryInfo(Uint4 query_length, BlastQueryInfo* *query_info)
{
    (*query_info) = BlastQueryInfoNew(eBlastTypeBlastn, 1);
    (*query_info)->contexts[0].query_offset = 0;
    (*query_info)->contexts[0].query_length = query_length;
    (*query_info)->contexts[1].query_offset = query_length + 1;
    (*query_info)->contexts[1].query_length = query_length;
    (*query_info)->max_length = query_length;
}

struct LinkHspTestFixture {

    EBlastProgramType m_ProgramType;
    EProgram m_Program;
    BlastHSPList* m_HspList;
    BlastScoreBlk* m_ScoreBlk;
    CBlastQueryInfo m_QueryInfo;
    Int4 m_SubjectLength;
    BlastHitSavingParameters* m_HitParams;

    ~LinkHspTestFixture() {
        freeStructures();
    }

    /// Sets up the input list of HSPs. These must be sorted by score.
    void setupHSPListTransl()
    {
        const int kNumHsps = 10;
        const int kScores[kNumHsps] = 
            { 1023, 282, 246, 202, 142, 117, 98, 92, 63, 53 };
        const int kQueryOffsets[kNumHsps] = 
            { 11, 346, 399, 244, 287, 224, 311, 218, 0, 404};
        const int kQueryLengths[kNumHsps] = 
            { 244, 56, 49, 49, 104, 29, 36, 37, 12, 25 };
        const int kSubjectFrames[kNumHsps] = 
            { 2, 2, 3, 2, 1, 1, 2, 3, 3, 2 };
        const int kSubjectOffsets[kNumHsps] = 
            { 1372, 2677, 2756, 2062, 2209, 1832, 2351, 1732, 1140, 2683 };
        const int kSubjectLengths[kNumHsps] = 
            {300, 56, 49, 50, 75, 29, 32, 36, 12, 26 };

        m_HspList = Blast_HSPListNew(0);       
        Int4 index;
        BlastHSP* hsp;

        for (index = 0; index < kNumHsps; ++index) {
            m_HspList->hsp_array[index] = hsp = 
                (BlastHSP*) calloc(1, sizeof(BlastHSP));
            hsp->score = kScores[index];
            if (m_ProgramType == eBlastTypeTblastn) {
                hsp->query.offset = kQueryOffsets[index];
                hsp->query.end = kQueryOffsets[index] + kQueryLengths[index];
                hsp->subject.offset = kSubjectOffsets[index];
                hsp->subject.end = 
                    kSubjectOffsets[index] + kSubjectLengths[index];
                hsp->subject.frame = kSubjectFrames[index];
            } else {
                hsp->query.offset = kSubjectOffsets[index];
                hsp->query.end = 
                    kSubjectOffsets[index] + kSubjectLengths[index];
                hsp->subject.offset = kQueryOffsets[index];
                hsp->subject.end = kQueryOffsets[index] + kQueryLengths[index];
                hsp->query.frame = kSubjectFrames[index];
            }
        }

        m_HspList->hspcnt = kNumHsps;
    }

    /// Sets up the scoring block with the Karlin-Altschul parameters
    void setupScoreBlk(Uint1* seqbuf, bool gapped,
                       BlastScoringOptions** score_options_ptr) 
    {
        Int2 status;
        BlastScoringOptions* score_options = NULL;
        m_ScoreBlk = 
            BlastScoreBlkNew((m_ProgramType==eBlastTypeBlastn ? 
                              BLASTNA_SEQ_CODE : BLASTAA_SEQ_CODE), 
                             m_QueryInfo->last_context+1);

        BlastScoringOptionsNew(m_ProgramType, &score_options);
        score_options->gapped_calculation = (gapped ? TRUE : FALSE);

        if (m_ProgramType != eBlastTypeBlastn) {
            BOOST_REQUIRE(!strcmp("BLOSUM62", score_options->matrix));
        }
        status = Blast_ScoreBlkMatrixInit(m_ProgramType, score_options, 
            m_ScoreBlk, &BlastFindMatrixPath);

        BOOST_REQUIRE(status == 0);

        Blast_Message* message = NULL;
        status = Blast_ScoreBlkKbpUngappedCalc(m_ProgramType, m_ScoreBlk, 
                                           seqbuf, m_QueryInfo, &message);
        message = Blast_MessageFree(message);

        BOOST_REQUIRE(status == 0);

        if (gapped) {
            status = Blast_ScoreBlkKbpGappedCalc(m_ScoreBlk, score_options, 
                                         m_ProgramType, m_QueryInfo, NULL);
            BOOST_REQUIRE(status == 0);
            m_ScoreBlk->kbp_gap = m_ScoreBlk->kbp_gap_std;
        }

        m_ScoreBlk->kbp = m_ScoreBlk->kbp_std;

        if (score_options_ptr)
            *score_options_ptr = score_options;
        else
            BlastScoringOptionsFree(score_options);
    }

    /// Sets up the hit saving parameters structures. Only the fields relevant
    /// to linking HSPs are filled.
    void setupHitParams(int longest_intron, double evalue)
    {
        int cutoff_small_gap = (m_ProgramType == eBlastTypeBlastn ? 16 : 42);
        m_HitParams = 
            (BlastHitSavingParameters*) calloc(1, sizeof(BlastHitSavingParameters));
        m_HitParams->options = (BlastHitSavingOptions *)
            calloc(1, sizeof(BlastHitSavingOptions));
        m_HitParams->options->expect_value = evalue;
        BlastLinkHSPParametersNew(m_ProgramType, TRUE,
                                  &m_HitParams->link_hsp_params);
        m_HitParams->link_hsp_params->cutoff_big_gap = 0;
        m_HitParams->link_hsp_params->cutoff_small_gap = cutoff_small_gap;
        m_HitParams->link_hsp_params->longest_intron = longest_intron;
    }

    /// Fills the effective lengths data into the query information structure
    void 
    fillEffectiveLengths(const BlastScoringOptions* score_options,
                         Int8 db_length, Int4 db_num_seq)
    {
        BlastEffectiveLengthsOptions* eff_len_options = NULL;
        BlastEffectiveLengthsOptionsNew(&eff_len_options);
        BlastEffectiveLengthsParameters* eff_len_params = NULL;
        BlastEffectiveLengthsParametersNew(eff_len_options, db_length, 
                                           db_num_seq, &eff_len_params);
        BLAST_CalcEffLengths(m_ProgramType, score_options, eff_len_params, 
                             m_ScoreBlk, m_QueryInfo, NULL);
        BlastEffectiveLengthsParametersFree(eff_len_params);
        BlastEffectiveLengthsOptionsFree(eff_len_options);
    }

    /// Complete set-up before calling the HSP linking algorithm
    void setupLinkHspInputTblastn()
    {
        const string kProtGi = "9930103";
        const string kNuclGi = "9930102";
        const Uint4 kProtLength = 448;
        const Uint4 kNuclLength = 8872;

        string qid_str = "gi|" + ((m_ProgramType == eBlastTypeTblastn) ? 
                                  kProtGi : kNuclGi);
        CSeq_id query_id(qid_str);
        TSeqLocVector query_v;

        if (m_ProgramType == eBlastTypeBlastx) {
            auto_ptr<SSeqLoc> qsl(
                CTestObjMgr::Instance().CreateSSeqLoc(query_id, 
                                                      eNa_strand_both));
            query_v.push_back(*qsl);
        } else {
            auto_ptr<SSeqLoc> qsl(
                CTestObjMgr::Instance().CreateSSeqLoc(query_id));
            query_v.push_back(*qsl);
        }

        CBlastOptions options;
        options.SetStrandOption(eNa_strand_unknown);
        if (m_ProgramType == eBlastTypeBlastx)
            options.SetQueryGeneticCode(1);
        
        options.SetProgram(m_Program);
        CBLAST_SequenceBlk query_blk;
        TSearchMessages blast_msg;

        ENa_strand strand_opt = options.GetStrandOption();
 
        SetupQueryInfo(query_v, m_ProgramType, strand_opt, &m_QueryInfo);
        SetupQueries(query_v, m_QueryInfo, &query_blk, 
                     m_ProgramType, strand_opt, blast_msg);
        ITERATE(TSearchMessages, m, blast_msg) {
            BOOST_REQUIRE(m->empty());
        }

        BlastScoringOptions* score_options = NULL;
        setupScoreBlk(query_blk->sequence, true, &score_options);

        m_SubjectLength = (m_ProgramType == eBlastTypeTblastn ?
                           kNuclLength / 3 : kProtLength);
        
        fillEffectiveLengths(score_options, (Int8)m_SubjectLength, 1);
        BlastScoringOptionsFree(score_options);

    }
    
    /// Frees all the C structures used in the test
    void freeStructures()
    {
        m_HspList = Blast_HSPListFree(m_HspList);

        if (m_HitParams) {
            BlastHitSavingOptionsFree(m_HitParams->options);
            m_HitParams = BlastHitSavingParametersFree(m_HitParams);
        }
        m_ScoreBlk = BlastScoreBlkFree(m_ScoreBlk);
    }

    /// Test linking with uneven gap sum statistics
    void testUnevenGapLinkHsps() {
        const int kNumHsps = 8;
        const int kLongestIntron = 4000;
        const double kEvalue = 1e-10;
        const int kNumsLinked[kNumHsps] = { 1, 5, 5, 5, 2, 5, 5, 2 };
        const int kScores[kNumHsps] = { 1023, 282, 246, 202, 142, 117, 98, 92 };

        setupLinkHspInputTblastn();
        setupHSPListTransl();
        setupHitParams(kLongestIntron, kEvalue);

        BLAST_LinkHsps(m_ProgramType, m_HspList, m_QueryInfo, m_SubjectLength, 
                       m_ScoreBlk, m_HitParams->link_hsp_params, TRUE);

        Blast_HSPListReapByEvalue(m_HspList, m_HitParams->options);

        BOOST_REQUIRE_EQUAL(kNumHsps, m_HspList->hspcnt);

        for (int index = 0; index < kNumHsps; ++index) {
            BOOST_REQUIRE_EQUAL(kNumsLinked[index], m_HspList->hsp_array[index]->num);
            BOOST_REQUIRE_EQUAL(kScores[index], m_HspList->hsp_array[index]->score); 
        }
    }

    void setupHSPListForMiddleInsertTest()
    {
        const int kNumHsps = 5;
        const int kScores[kNumHsps] =
            { 80, 60, 55, 54, 52 };
        const int kQueryOffsets[kNumHsps] =
            { 100, 130, 239, 239, 191 };
        const int kLengths[kNumHsps] =
            { 100, 50, 100, 9, 57 };
        const int kSubjectOffsets[kNumHsps] =
            { 1100, 1130, 3240, 3240, 2195 };

        m_HspList = Blast_HSPListNew(0);
        Int4 index;
        BlastHSP* hsp;

        for (index = 0; index < kNumHsps; ++index) {
            m_HspList->hsp_array[index] = hsp =
                (BlastHSP*) calloc(1, sizeof(BlastHSP));
            hsp->score = kScores[index];
            hsp->query.offset = kQueryOffsets[index];
            hsp->subject.offset = kSubjectOffsets[index];
            hsp->subject.frame = 1;
            hsp->query.end = hsp->query.offset + kLengths[index];
            hsp->subject.end = hsp->subject.offset + kLengths[index];
        }

        m_HspList->hspcnt = kNumHsps;
    }

    /// HSP list setup for blastn
    void setupHSPListNucl()
    {
       const int kNumHsps = 8;
       const int kScores[kNumHsps] = { 35, 31, 22, 21, 20, 20, 20, 20 };
       const int kQueryFrames[kNumHsps] = { 1, 1, 1, -1, 1, -1, -1, -1 };
       const int kQueryStarts[kNumHsps] = 
           { 790, 790, 791, 4606, 870, 4572, 4526, 4589 }; 
       const int kQueryEnds[kNumHsps] = 
           { 865, 865, 833, 4635, 894, 4604, 4550, 4629 };
       const int kSubjectStarts[kNumHsps] = 
           { 453, 3469, 5837, 12508, 5951, 11005, 9899, 7397 };
       const int kSubjectEnds[kNumHsps] = 
           { 528, 3544, 5879, 12537, 5975, 11037, 9923, 7437 };
       Int4 index;
       BlastHSP* hsp;

       m_HspList = Blast_HSPListNew(0);       

       for (index = 0; index < kNumHsps; ++index) {
      hsp = m_HspList->hsp_array[index] = 
         (BlastHSP*) calloc(1, sizeof(BlastHSP));
      hsp->score = kScores[index];
      hsp->query.offset = kQueryStarts[index];
      hsp->query.end = kQueryEnds[index];
      hsp->query.frame = kQueryFrames[index];
      hsp->context = (kQueryFrames[index] > 0 ? 0 : 1);
      hsp->subject.offset = kSubjectStarts[index];
      hsp->subject.end = kSubjectEnds[index];
      hsp->subject.frame = 1;
       }
       m_HspList->hspcnt = kNumHsps;
    }

    /// Complete set-up before calling the HSP linking algorithm
    void setupLinkHspInputBlastn()
    {
        const Uint4 kQueryLength = 5419;
        const Int8 kEffDbLength = 122632232;

        m_ProgramType = eBlastTypeBlastn;
        m_Program = eBlastn;
        
        // In subject sequence block, we only need to fill sequence length.
        s_SetupNuclQueryInfo(kQueryLength, &m_QueryInfo); 
        m_SubjectLength = 12991;

        CSeq_id seqid("gi|24638835");
        pair<TSeqPos, TSeqPos> range(26993,32411);

        auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(seqid, range));

        SBlastSequence sequence(
            GetSequence(*sl->seqloc, eBlastEncodingNucleotide,
                        sl->scope, eNa_strand_both, eSentinels));
        BlastScoringOptions* score_options = NULL;
        setupScoreBlk(sequence.data.get(), false, &score_options);
        
        fillEffectiveLengths(score_options, kEffDbLength, 1);
        BlastScoringOptionsFree(score_options);

        setupHSPListNucl();
    }

    AllCutoffScores* 
    setupCutoffScores(bool gapped, Int8 db_length, Uint4 db_num_seq,
                      Uint4 subj_length, int longest_intron=0)
    {
        BlastInitialWordOptions* word_options = NULL;
        BlastExtensionOptions* ext_options = NULL;
        BlastHitSavingOptions* hit_options = NULL;

        BlastInitialWordOptionsNew(m_ProgramType, &word_options);
        BlastExtensionOptionsNew(m_ProgramType, &ext_options, true);
        if (m_ProgramType == eBlastTypeBlastn) {
            word_options->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_NUCL;
            ext_options->gap_x_dropoff = BLAST_GAP_X_DROPOFF_NUCL;
            ext_options->gap_x_dropoff_final = BLAST_GAP_X_DROPOFF_FINAL_NUCL;
        }
        BlastHitSavingOptionsNew(m_ProgramType, &hit_options, gapped);
        if (longest_intron > 0)
             hit_options->longest_intron = longest_intron;

        BlastInitialWordParameters* word_params = NULL;
        BlastExtensionParameters* ext_params = NULL;

        CRef<CSeq_id> qid;
        TSeqLocVector qv;
        
        if (m_ProgramType == eBlastTypeBlastn || m_ProgramType == eBlastTypeBlastx || 
            m_ProgramType == eBlastTypeTblastx) {
            qid.Reset(new CSeq_id("gi|555"));
            auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(*qid, 
                                                             eNa_strand_both));
            qv.push_back(*qsl);
        } else {
            qid.Reset(new CSeq_id("gi|129295"));
            auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(*qid));
            qv.push_back(*qsl);
        }

        CBlastOptions options;
        options.SetStrandOption(eNa_strand_unknown);
        if (m_ProgramType == eBlastTypeBlastx || 
            m_ProgramType == eBlastTypeTblastx)
            options.SetQueryGeneticCode(1);

        options.SetProgram(m_Program);
        CBLAST_SequenceBlk query_blk;
        TSearchMessages blast_msg;

        ENa_strand strand_opt = options.GetStrandOption();

        SetupQueryInfo(qv, m_ProgramType, strand_opt, &m_QueryInfo);
        SetupQueries(qv, m_QueryInfo, &query_blk, 
                     m_ProgramType, strand_opt, blast_msg);
        ITERATE(TSearchMessages, m, blast_msg) {
            BOOST_REQUIRE(m->empty());
        }

        BlastScoringOptions* score_options = NULL;
        setupScoreBlk(query_blk->sequence, gapped, &score_options);

        BlastExtensionParametersNew(m_ProgramType, ext_options, m_ScoreBlk, 
                                    m_QueryInfo, &ext_params);
        fillEffectiveLengths(score_options, (Int8)db_length, db_num_seq);
        score_options = BlastScoringOptionsFree(score_options);

        BlastHitSavingParametersNew(m_ProgramType, hit_options,
                                    m_ScoreBlk, m_QueryInfo, subj_length, 0, &m_HitParams);


        QuerySetUpOptions* query_options = NULL;
        BlastQuerySetUpOptionsNew(&query_options);
        LookupTableWrap* lookup_wrap = NULL;
        LookupTableOptions* lookup_options = NULL;
        BlastSeqLoc* blast_seq_loc = BlastSeqLocNew(NULL, 0, m_QueryInfo->contexts[0].query_length-1);
        LookupTableOptionsNew(m_ProgramType, &lookup_options);
        LookupTableWrapInit(query_blk, lookup_options, query_options, blast_seq_loc, m_ScoreBlk, &lookup_wrap, NULL, NULL);
        query_options = BlastQuerySetUpOptionsFree(query_options);

        Uint4 avg_subj_length = (Uint4)(db_length/db_num_seq);
        BlastInitialWordParametersNew(m_ProgramType, word_options, m_HitParams, lookup_wrap, 
           m_ScoreBlk, m_QueryInfo, avg_subj_length, &word_params);

        blast_seq_loc = BlastSeqLocFree(blast_seq_loc);
        lookup_wrap = LookupTableWrapFree(lookup_wrap);
        lookup_options = LookupTableOptionsFree(lookup_options);

        BlastLinkHSPParametersUpdate(word_params, m_HitParams, (gapped ? TRUE : FALSE));

        
        if (m_HitParams->link_hsp_params && 
            m_ProgramType != eBlastTypeBlastn && !gapped) {
            CalculateLinkHSPCutoffs(m_ProgramType, m_QueryInfo, m_ScoreBlk, 
               m_HitParams->link_hsp_params, word_params, db_length, 
               subj_length); 
        }

        AllCutoffScores* retval = 
            (AllCutoffScores*) calloc(1, sizeof(AllCutoffScores));
        retval->x_drop_ungapped = word_params->x_dropoff_max;
        retval->x_drop_gapped = ext_params->gap_x_dropoff;
        retval->x_drop_final = ext_params->gap_x_dropoff_final;
        retval->cutoff_score_ungapped = word_params->cutoff_score_min;
        retval->cutoff_score_final = m_HitParams->cutoff_score_min;
        retval->do_sum_stats = m_HitParams->do_sum_stats;
        if (retval->do_sum_stats) {
            retval->cutoff_small_gap = 
                m_HitParams->link_hsp_params->cutoff_small_gap;
            retval->cutoff_big_gap = 
                m_HitParams->link_hsp_params->cutoff_big_gap;
        }

        BlastInitialWordParametersFree(word_params);
        BlastInitialWordOptionsFree(word_options);
        BlastExtensionParametersFree(ext_params);
        BlastExtensionOptionsFree(ext_options);
        // Set to NULL those member fields that are not used in these tests.
        m_HspList = NULL;

        return retval;
    }

};

BOOST_FIXTURE_TEST_SUITE(linkhsp, LinkHspTestFixture)

/// Test linking with uneven gap sum statistics
BOOST_AUTO_TEST_CASE(testUnevenGapLinkHspsTblastn) {
    m_ProgramType = eBlastTypeTblastn;
    m_Program = eTblastn;
    testUnevenGapLinkHsps();
}

/// Test linking with uneven gap sum statistics
BOOST_AUTO_TEST_CASE(testUnevenGapLinkHspsBlastx) {
    m_ProgramType = eBlastTypeBlastx;
    m_Program = eBlastx;
    testUnevenGapLinkHsps();
}

/// Tests the uneven gap linking where an HSP has to be inserted in the 
/// middle between two higher scoring HSPs that can be linked by themselves.
BOOST_AUTO_TEST_CASE(testUnevenGapLinkHspsMiddleInsertion) {
    const int kNumHsps = 5;
    const int kLongestIntron = 3000;
    const double kEvalue = 10;
    const int kLinkNums[kNumHsps] = { 3, 1, 3, 1, 3 };
    m_ProgramType = eBlastTypeTblastn;
    m_Program = eTblastn;

    setupLinkHspInputTblastn();
    setupHSPListForMiddleInsertTest();
    setupHitParams(kLongestIntron, kEvalue);

    BLAST_LinkHsps(m_ProgramType, m_HspList, m_QueryInfo, m_SubjectLength, 
                   m_ScoreBlk, m_HitParams->link_hsp_params, TRUE);
    for (int index = 0; index < m_HspList->hspcnt; ++index) {
        BOOST_REQUIRE_EQUAL(kLinkNums[index], 
                             m_HspList->hsp_array[index]->num);
    }
}

/// Test linking with small/large gap sum statistics for tblastn
BOOST_AUTO_TEST_CASE(testEvenGapLinkHspsTblastn) {
    const int kNumHsps = 5;
    const double kEvalue = 1e-10;
    const int kNumsLinked[kNumHsps] = { 1, 2, 2, 1, 1 };
    const int kScores[kNumHsps] = { 1023, 282, 246, 202, 142 };

    m_ProgramType = eBlastTypeTblastn;
    m_Program = eTblastn;
    setupLinkHspInputTblastn();
    setupHSPListTransl();

    setupHitParams(0, kEvalue);

    BLAST_LinkHsps(m_ProgramType, m_HspList, m_QueryInfo, m_SubjectLength, 
                   m_ScoreBlk, m_HitParams->link_hsp_params, TRUE);

    Blast_HSPListReapByEvalue(m_HspList, m_HitParams->options);

    BOOST_REQUIRE_EQUAL(kNumHsps, m_HspList->hspcnt);

    Int4 index;
    for (index = 0; index < kNumHsps; ++index) {
        BOOST_REQUIRE_EQUAL(kNumsLinked[index], m_HspList->hsp_array[index]->num);
        BOOST_REQUIRE_EQUAL(kScores[index], 
                             m_HspList->hsp_array[index]->score); 
    }
}

/// Test linking with small/large gap sum statistics for blastn
BOOST_AUTO_TEST_CASE(testEvenGapLinkHspsBlastn) {
    const int kNumHsps = 8;
    const double kEvalue = 10;
    const int kNumsLinked[kNumHsps] = 
    { 2, 1, 1, 3, 2, 1, 3, 3 };
    const double kEvalues[kNumHsps] = 
    { 3e-12, 3e-7, 0.07, 1e-7, 3e-12, 1.1, 1e-7, 1e-7 };

    setupLinkHspInputBlastn();
    setupHitParams(0, kEvalue);

    BLAST_LinkHsps(m_ProgramType, m_HspList, m_QueryInfo, m_SubjectLength, 
                   m_ScoreBlk, m_HitParams->link_hsp_params, FALSE);

    Blast_HSPListReapByEvalue(m_HspList, m_HitParams->options);
    BOOST_REQUIRE_EQUAL(kNumHsps, m_HspList->hspcnt);

    for (Int4 index = 0; index < kNumHsps; ++index) {
        BOOST_REQUIRE_EQUAL(kNumsLinked[index], 
                             m_HspList->hsp_array[index]->num);
        BOOST_REQUIRE(fabs(kEvalues[index] - m_HspList->hsp_array[index]->evalue)/kEvalues[index] < 0.5); 
    }
}

static void 
testAllCutoffs(const AllCutoffScores& good_cutoffs, 
               AllCutoffScores& cutoffs)
{
    BOOST_REQUIRE_EQUAL(good_cutoffs.x_drop_ungapped, 
                         cutoffs.x_drop_ungapped);
    BOOST_REQUIRE_EQUAL(good_cutoffs.x_drop_gapped, 
                         cutoffs.x_drop_gapped);
    BOOST_REQUIRE_EQUAL(good_cutoffs.x_drop_final, 
                         cutoffs.x_drop_final);
    BOOST_REQUIRE_EQUAL(good_cutoffs.cutoff_score_ungapped, 
                         cutoffs.cutoff_score_ungapped);
    BOOST_REQUIRE_EQUAL(good_cutoffs.cutoff_score_final, 
                         cutoffs.cutoff_score_final);
    BOOST_REQUIRE_EQUAL(good_cutoffs.do_sum_stats, 
                         cutoffs.do_sum_stats);
    BOOST_REQUIRE_EQUAL(good_cutoffs.cutoff_small_gap, 
                         cutoffs.cutoff_small_gap);
    BOOST_REQUIRE_EQUAL(good_cutoffs.cutoff_big_gap, 
                         cutoffs.cutoff_big_gap);
}

BOOST_AUTO_TEST_CASE(UngappedBlastnCutoffs)
{
    const int kNumDbs = 4;
    const Int8 kDbLengths[kNumDbs] = 
        { 10000000000LL, 10000000000LL, 3000000000LL, 10000LL };
    const Uint4 kDbNumSeqs[kNumDbs] = { 2000000, 20000000, 500, 100 };
    const Uint4 kSubjectLengths[kNumDbs] = { 2000, 400, 3000000, 100 };
    const AllCutoffScores kGoodCutoffs[kNumDbs] = { 
        { 11, 0, 0, 0, 14, 20, true, 14, 0 },
        { 11, 0, 0, 0, 12, 20, true, 12, 0 },
        { 11, 0, 0, 0, 19, 19, true, 19, 0 },
        { 11, 0, 0, 0, 10, 10, true, 10, 0 } };
    
    AllCutoffScores* cutoffs = NULL;
    int index;
    m_ProgramType = eBlastTypeBlastn;
    m_Program = eBlastn;
    for (index = 0; index < kNumDbs; ++index) { 
        cutoffs = setupCutoffScores(false, kDbLengths[index], 
                     kDbNumSeqs[index], kSubjectLengths[index]);
        testAllCutoffs(kGoodCutoffs[index], *cutoffs);
        sfree(cutoffs);
        freeStructures();
        if (index < kNumDbs-1)
            BlastQueryInfoFree(m_QueryInfo);
    }
}

BOOST_AUTO_TEST_CASE(UngappedBlastpCutoffs)
{
    const Int8 kDbLength =  500000000;
    const Uint4 kDbNumSeqs = 1000000;
    const int kNumSubjects = 3;
    const Uint4 kSubjectLengths[kNumSubjects] = {400, 60, 3000 };
    const AllCutoffScores kGoodCutoffs[kNumSubjects] = { 
        { 16, 0, 0, 0, 41, 66, true, 41, 38 },
        { 16, 0, 0, 0, 41, 66, true, 0, 29 },
        { 16, 0, 0, 0, 41, 66, true, 41, 44 } };
    AllCutoffScores* cutoffs = NULL;
    int index;
    m_ProgramType = eBlastTypeBlastp;
    m_Program = eBlastp;
    for (index = 0; index < kNumSubjects; ++index) { 
        cutoffs = setupCutoffScores(false, kDbLength,
                          kDbNumSeqs, kSubjectLengths[index]);
        testAllCutoffs(kGoodCutoffs[index], *cutoffs);
        sfree(cutoffs);
        freeStructures();
        if (index < kNumSubjects-1)
            BlastQueryInfoFree(m_QueryInfo);
    }
}

BOOST_AUTO_TEST_CASE(UngappedBlastxCutoffs)
{
    const Int8 kDbLength =  /*500000000*/227102922;
    const Uint4 kDbNumSeqs = /*1000000*/761886;
    const int kNumSubjects = 3;
    const Uint4 kSubjectLengths[kNumSubjects] = { 400, 100, 3000 };
    const AllCutoffScores kGoodCutoffs[kNumSubjects] = { 
        { 16, 0, 0, 0, 31, 63, true, 31, 37 },
        { 16, 0, 0, 0, 31, 63, true,  0, 31 },
        { 16, 0, 0, 0, 31, 63, true, 31, 43 } };
    AllCutoffScores* cutoffs = NULL;
    int index;
    m_ProgramType = eBlastTypeBlastx;
    m_Program = eBlastx;
    for (index = 0; index < kNumSubjects; ++index) {  
        cutoffs = setupCutoffScores(false, kDbLength, kDbNumSeqs,
                                    kSubjectLengths[index]);
        testAllCutoffs(kGoodCutoffs[index], *cutoffs);
        sfree(cutoffs);
        freeStructures();
        if (index < kNumSubjects-1)
            BlastQueryInfoFree(m_QueryInfo);
    }
}

BOOST_AUTO_TEST_CASE(UngappedTblastnCutoffs)
{
    const int kNumDbs = 3;
    const Int8 kDbLengths[kNumDbs] = 
        { 10000000000LL, 10000000000LL, 3000000000LL };
    const Uint4 kDbNumSeqs[kNumDbs] = { 2000000, 20000000, 500 };
    const Uint4 kSubjectLengths[kNumDbs] = { 2000, 400, 3000000 };
    const AllCutoffScores kGoodCutoffs[kNumDbs] = { 
        { 16, 0, 0, 0, 40, 72, true, 40, 40 },
        { 16, 0, 0, 0, 33, 71, true, 33, 35 },
        { 16, 0, 0, 0, 41, 69, true, 41, 60 } };

    AllCutoffScores* cutoffs = NULL;
    int index;
    m_ProgramType = eBlastTypeTblastn;
    m_Program = eTblastn;
    for (index = 0; index < kNumDbs; ++index) { 
        cutoffs = setupCutoffScores(false, kDbLengths[index],
                          kDbNumSeqs[index], kSubjectLengths[index]);
        testAllCutoffs(kGoodCutoffs[index], *cutoffs);
        sfree(cutoffs);
        freeStructures();
        if (index < kNumDbs-1)
            BlastQueryInfoFree(m_QueryInfo);
    }
}

BOOST_AUTO_TEST_CASE(UngappedTblastxCutoffs)
{
    const int kNumDbs = 4;
    const Int8 kDbLengths[kNumDbs] = 
        { 10000000000LL, 10000000000LL, 10000000000LL, 3000000000LL };
    const Uint4 kDbNumSeqs[kNumDbs] = { 2000000, 2000000, 20000000, 500 };
    const Uint4 kSubjectLengths[kNumDbs] = { 2000, 100, 400, 3000000 };
    const AllCutoffScores kGoodCutoffs[kNumDbs] = { 
        { 16, 0, 0, 0, 41, 72, true, 41, 40 },
        { 16, 0, 0, 0, 41, 72, true,  0, 27 },
        { 16, 0, 0, 0, 41, 70, true, 41, 34 },
        { 16, 0, 0, 0, 41, 68, true, 41, 60 } };

    AllCutoffScores* cutoffs = NULL;
    int index;
    m_ProgramType = eBlastTypeTblastx;
    m_Program = eTblastx;
    for (index = 0; index < kNumDbs; ++index) { 
        cutoffs = setupCutoffScores(false, kDbLengths[index],
                          kDbNumSeqs[index], kSubjectLengths[index]);
        testAllCutoffs(kGoodCutoffs[index], *cutoffs);
        sfree(cutoffs);
        freeStructures();
        if (index < kNumDbs-1)
            BlastQueryInfoFree(m_QueryInfo);
    }
}

BOOST_AUTO_TEST_CASE(GappedBlastnCutoffs)
{
    const int kNumDbs = 4;
    const Int8 kDbLengths[kNumDbs] = 
        { 10000000000LL, 10000000000LL, 3000000000LL, 10000LL };
    const Uint4 kDbNumSeqs[kNumDbs] = { 2000000, 20000000, 500, 200 };
    const Uint4 kSubjectLengths[kNumDbs] = { 2000, 400, 3000000, 60 };
    const AllCutoffScores kGoodCutoffs[kNumDbs] = { 
        { 11, 15, 50, 0, 13, 20, false, 0, 0 },
        { 11, 15, 50, 0, 13, 20, false, 0, 0 },
        { 11, 15, 50, 0, 13, 19, false, 0, 0 },
        { 11, 15, 50, 0, 10, 10, false, 0, 0 } };

    AllCutoffScores* cutoffs = NULL;
    int index;
    m_ProgramType = eBlastTypeBlastn;
    m_Program = eBlastn;
    for (index = 0; index < kNumDbs; ++index) { 
        cutoffs = setupCutoffScores(true, kDbLengths[index],
                          kDbNumSeqs[index], kSubjectLengths[index]);
        testAllCutoffs(kGoodCutoffs[index], *cutoffs);
        sfree(cutoffs);
        freeStructures();
        if (index < kNumDbs-1)
            BlastQueryInfoFree(m_QueryInfo);
    }
}

BOOST_AUTO_TEST_CASE(GappedBlastpCutoffs)
{
    const Int8 kDbLength = 600000000;
    const Uint4 kDbNumSeqs = 1800000;
    const Uint4 kSubjectLength = 200;
    m_ProgramType = eBlastTypeBlastp;
    m_Program = eBlastp;
    const AllCutoffScores kGoodCutoffs =
        { 16, 38, 64, 41, 19, 19, false, 0, 0 };
    AllCutoffScores* cutoffs = 
        setupCutoffScores(true, kDbLength, kDbNumSeqs, kSubjectLength);
    testAllCutoffs(kGoodCutoffs, *cutoffs);
    sfree(cutoffs);
    freeStructures();
}

BOOST_AUTO_TEST_CASE(GappedBlastxCutoffs)
{
    const int kNumDbs = 2;
    const Int8 kDbLengths[kNumDbs] = 
          {600000000, 6000000000LL};
    const Uint4 kDbNumSeqs = 1800000;
    const Uint4 kSubjectLength[kNumDbs] = {500, 2000};
    const AllCutoffScores kGoodCutoffs[kNumDbs] = {
        { 16, 38, 64, 0, 22, 22, true, 22, 0 },
        { 16, 38, 64, 0, 27, 27, true, 27, 0 } };
    m_ProgramType = eBlastTypeBlastx;
    m_Program = eBlastx;
    for (int index = 0; index < kNumDbs; ++index) { 
        AllCutoffScores* cutoffs = setupCutoffScores(true, 
              kDbLengths[index], kDbNumSeqs, kSubjectLength[index]);
        testAllCutoffs(kGoodCutoffs[index], *cutoffs);
        sfree(cutoffs);
        freeStructures();
        if (index < kNumDbs-1)
            BlastQueryInfoFree(m_QueryInfo);
    }
}

BOOST_AUTO_TEST_CASE(GappedTblastnCutoffs)
{
    const int kNumDbs = 3;
    const Int8 kDbLengths[kNumDbs] = 
        { 10000000000LL, 10000000000LL, 3000000000LL };
    const Uint4 kDbNumSeqs[kNumDbs] = { 2000000, 20000000, 500 };
    const Uint4 kSubjectLengths[kNumDbs] = { 2000, 400, 3000000 };
    const AllCutoffScores kGoodCutoffs[kNumDbs] = { 
        { 16, 38, 64, 41, 27, 27, true, 27, 0 },
        { 16, 38, 64, 41, 21, 21, true, 21, 0 },
        { 16, 38, 64, 41, 41, 54, true, 41, 0 } };

    AllCutoffScores* cutoffs = NULL;
    int index;
    m_ProgramType = eBlastTypeTblastn;
    m_Program = eTblastn;
    for (index = 0; index < kNumDbs; ++index) { 
        cutoffs = setupCutoffScores(true, kDbLengths[index],
                          kDbNumSeqs[index], kSubjectLengths[index]);
        testAllCutoffs(kGoodCutoffs[index], *cutoffs);
        sfree(cutoffs);
        freeStructures();
        if (index < kNumDbs-1)
            BlastQueryInfoFree(m_QueryInfo);
    }
}

BOOST_AUTO_TEST_CASE(GappedTblastnVeryShortIntron)
{
    const int kNumDbs = 3;
    const Int8 kDbLengths[kNumDbs] = 
        { 10000000000LL, 10000000000LL, 3000000000LL };
    const Uint4 kDbNumSeqs[kNumDbs] = { 2000000, 20000000, 500 };
    const Uint4 kSubjectLengths[kNumDbs] = { 2000, 400, 3000000 };

    AllCutoffScores* cutoffs = NULL;
    int index;
    m_ProgramType = eBlastTypeTblastn;
    m_Program = eTblastn;
    for (index = 0; index < kNumDbs; ++index) { 
        cutoffs = setupCutoffScores(true, kDbLengths[index],
                          kDbNumSeqs[index], kSubjectLengths[index], 1);
        
        BOOST_REQUIRE_EQUAL((int) false, (int) cutoffs->do_sum_stats);
        sfree(cutoffs);
        freeStructures();
        if (index < kNumDbs-1)
            BlastQueryInfoFree(m_QueryInfo);
    }
}
BOOST_AUTO_TEST_SUITE_END()
