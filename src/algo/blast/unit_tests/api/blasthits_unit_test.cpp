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
*   Unit test module to test hit saving procedures in BLAST
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include "blast_setup.hpp"
#include "blast_objmgr_priv.hpp"
#include "test_objmgr.hpp"

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/gencode_singleton.h>
#include "blast_hits_priv.h"

extern "C" int h_score_compare_hsps(const void* v1, const void* v2)
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

using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

BOOST_AUTO_TEST_SUITE(blasthits)

    void setupHSPList(BlastHSPList** hsp_list_ptr, int chunk)
    {
        const int kNumLists = 3;
        const int kMaxHspCount = 6;
        const int kHspCounts[kNumLists] = { 6, 6, 4 };
        const int kSubjectOffsets[kNumLists][kMaxHspCount] = 
            { { 100, 9950, 5000, 9850, 3000, 9970 }, 
              { 9950, 9902, 9970, 10400, 19750, 19820 }, 
              { 19805, 25000, 19820, 22000 } };
        const int kQueryOffsets[kNumLists][kMaxHspCount] = 
            { { 100, 200, 300, 400, 500, 600 }, 
              { 200, 452, 600, 100, 200, 300 }, 
              { 255, 100, 300, 200 } };
        const int kLengths[kNumLists][kMaxHspCount] = 
            { { 100, 45, 100, 100, 100, 28 }, 
              { 100, 48, 100, 100, 100, 60 }, 
              { 45, 200, 60, 200} };
        const int kScores[kNumLists][kMaxHspCount] = 
            { { 60, 30, 70, 75, 65, 28 }, 
              { 80, 40, 62, 77, 72, 44 }, 
              { 32, 120, 44, 111 } };
        const int contexts[kNumLists][kMaxHspCount] = 
            { { 0, 1, 2, 1, 3, 1 }, 
              { 1, 4, 1, 0, 1, 1 }, 
              { 1, 0, 1, 4 } };

        int index;
        BlastHSPList* hsp_list = *hsp_list_ptr;
        hsp_list->hspcnt = kHspCounts[chunk];
        for (index = 0; index < kHspCounts[chunk]; ++index) {
            hsp_list->hsp_array[index] = 
                (BlastHSP*) calloc(1, sizeof(BlastHSP));
            hsp_list->hsp_array[index]->query.offset = kQueryOffsets[chunk][index];
            hsp_list->hsp_array[index]->subject.offset = kSubjectOffsets[chunk][index];
            hsp_list->hsp_array[index]->query.end = 
                kQueryOffsets[chunk][index] + kLengths[chunk][index];
            hsp_list->hsp_array[index]->subject.end = 
                kSubjectOffsets[chunk][index] + kLengths[chunk][index];
            hsp_list->hsp_array[index]->score = kScores[chunk][index];
            hsp_list->hsp_array[index]->context = contexts[chunk][index];
        }
    }


BOOST_AUTO_TEST_CASE(BlastTargetSequence)
{
    CAutomaticGenCodeSingleton instance;
    CSeq_id sid("CU856286");  // DNA sequence
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid)); 

    Int2 status = 0;
    SBlastSequence subj_sequence(
                    GetSequence(*subj->seqloc,
                                eBlastEncodingNcbi4na,
                                subj->scope,
                                eNa_strand_plus,
                                eSentinels));

    BLAST_SequenceBlk* subject_blk;
    BlastSeqBlkNew(&subject_blk);
    status = BlastSeqBlkSetSequence(subject_blk,
                            subj_sequence.data.release(), 
                            sequence::GetLength(*subj->seqloc, subj->scope)); 

    SBlastTargetTranslation* target_t = NULL;

    Uint1* gc_str =  GenCodeSingletonFind(1);
    status = BlastTargetTranslationNew(subject_blk, gc_str, eBlastTypeTblastn,
            false, &target_t);

    BOOST_REQUIRE_EQUAL(0, status);
    BOOST_REQUIRE(target_t);

    // HSP is NULL so nothing returned. 
    const Uint1* null_sequence = Blast_HSPGetTargetTranslation(target_t, NULL, NULL);
    BOOST_REQUIRE(null_sequence == NULL);

    const int kNumTests = 6;
    BlastHSP* hsp;
    Blast_HSPInit(1, 2000, 2000, 3000, 1, 2000, 0, 0, -3, 2000, NULL, &hsp);
    // These are values that come out for this case.  kLength could change if 
    // heuristic for allocating buffers in Blast_HSPGetTargetTranslation changes. 
    const int kLength[kNumTests] = {3899, 4099, 4300, 4500, 4699, 4899 }; 
    const int kValues[2*kNumTests] = {6, 16, 18, 10, 10, 6, 8, 4, 18, 17, 1, 6};
    int index = 0;
    for (index=0; index<kNumTests; index++)
    {
        Int4 length = 0;
        if (hsp->subject.frame == 0)
        	hsp->subject.frame++;
        const Uint1* sequence = Blast_HSPGetTargetTranslation(target_t, hsp, NULL); // Test without retrieving length.
        BOOST_REQUIRE_EQUAL(kValues[2*index], (int) sequence[hsp->subject.offset+5]);

        Int4 i, n = 0;
        for (i = hsp->subject.offset-701; i < hsp->subject.offset - 693; ++i) {
            if (sequence[i] == 201) ++n; 
        }
        BOOST_REQUIRE(n!=0);
        n = 0;
        for (; i < hsp->subject.end + 693; ++i) {
            if (sequence[i] == 201) ++n; 
        }
        BOOST_REQUIRE(n==0);
        n = 0;
        for (; i < hsp->subject.end + 701 && n == 0; ++i) {
            if (sequence[i] == 201) ++n; 
        }
        BOOST_REQUIRE(n!=0);

        hsp->subject.offset += 200;
        hsp->subject.end += 200;
        sequence = Blast_HSPGetTargetTranslation(target_t, hsp, &length);
        BOOST_REQUIRE_EQUAL(kLength[index], length);
        BOOST_REQUIRE_EQUAL(kValues[2*index+1], (int) sequence[hsp->subject.offset+5]);

        n = 0;
        for (i = hsp->subject.offset-701; i < hsp->subject.offset - 693; ++i) {
            if (sequence[i] == 201) ++n; 
        }
        BOOST_REQUIRE(n!=0);
        n = 0;
        for (; i < hsp->subject.end + 693; ++i) {
            if (sequence[i] == 201) ++n; 
        }
        BOOST_REQUIRE(n==0);
        n = 0;
        for (; i < hsp->subject.end + 701 && n == 0; ++i) {
            if (sequence[i] == 201) ++n; 
        }
        BOOST_REQUIRE(n!=0);

        // Testing full translation
        Int4 offset = hsp->subject.offset;
        hsp->subject.offset = -1;
        sequence = Blast_HSPGetTargetTranslation(target_t, hsp, &length);
        n = 0;
        for (i=0; i < length; ++i) {
            if (sequence[i] == 201) ++n;
        }
        BOOST_REQUIRE(n==0);
        hsp->subject.offset = offset;
        hsp->subject.frame++;
    }

    hsp = Blast_HSPFree(hsp);
    subject_blk = BlastSequenceBlkFree(subject_blk);
    target_t = BlastTargetTranslationFree(target_t);
}


    BOOST_AUTO_TEST_CASE(testMergeHSPLists) {
        const int kNumChunks = 3;
        const int kOffsetIncrement = 9900;
        const int kTotalNumHsps = 12;
        const int kFinalScores[kTotalNumHsps] = 
            { 120, 111, 80, 77, 75, 72, 70, 65, 62, 60, 44, 40 };
        const int kFinalOffsets[kTotalNumHsps] = 
            { 25000, 22000, 9950, 10400, 9850, 19750, 5000, 3000, 9970, 100, 
              19820, 9902 };
        const int kFinalLengths[kTotalNumHsps] = 
            { 200, 200, 100, 100, 100, 100, 100, 100, 100, 100, 60, 48 };
        Int4 offset = 0, hsp_num_max = INT4_MAX;
        int chunk;
        BlastHSPList* combined_hsp_list = NULL;

        for (chunk = 0; chunk < kNumChunks; ++chunk) {
            BlastHSPList* hsp_list = Blast_HSPListNew(0);
            setupHSPList(&hsp_list, chunk);
            Blast_HSPListsMerge(&hsp_list, &combined_hsp_list, 
                                hsp_num_max, &offset, INT4_MIN,
                                DBSEQ_CHUNK_OVERLAP, TRUE);
            offset += kOffsetIncrement;
        }

        BOOST_REQUIRE_EQUAL(kTotalNumHsps, combined_hsp_list->hspcnt);
        qsort(combined_hsp_list->hsp_array, kTotalNumHsps, sizeof(BlastHSP*),
              h_score_compare_hsps);
        int index;
        for (index = 0; index < kTotalNumHsps; ++index) {
            BlastHSP* hsp = combined_hsp_list->hsp_array[index];
            BOOST_REQUIRE_EQUAL(kFinalScores[index], hsp->score);
            BOOST_REQUIRE_EQUAL(kFinalOffsets[index], hsp->subject.offset);
            BOOST_REQUIRE_EQUAL(kFinalLengths[index], 
                                 hsp->subject.end - hsp->subject.offset);
        }

        combined_hsp_list = Blast_HSPListFree(combined_hsp_list);
    }

    /* Test that purge of newly created HSPList works. */
    BOOST_AUTO_TEST_CASE(testPurgeOfNewHSPList) {
       BlastHSPList* hsp_list = Blast_HSPListNew(0);
       Blast_HSPListPurgeNullHSPs(hsp_list);
       /* Nothing was put on list, nothing should be purged off. */
       BOOST_REQUIRE_EQUAL(0, hsp_list->hspcnt);
       hsp_list = Blast_HSPListFree(hsp_list);
    }

    /* Create new HSPList and set the hspcnt to 5, even though all HSP's are NULL.
       Check that the hspcnt is reset to zero at the end. */
    BOOST_AUTO_TEST_CASE(testPurgeOfEmptyHSPList) {
       BlastHSPList* hsp_list = Blast_HSPListNew(0);
       const int kNumNullHsps=5; 
       hsp_list->hspcnt = kNumNullHsps;
       Blast_HSPListPurgeNullHSPs(hsp_list);
       /* Nothing was put on list, nothing should be purged off. */
       BOOST_REQUIRE_EQUAL(0, hsp_list->hspcnt);
       hsp_list = Blast_HSPListFree(hsp_list);
    }

    /* Check that an HSPList with no missing HSP's is not affected. */
    BOOST_AUTO_TEST_CASE(testPurgeOfFullHSPList) {
       BlastHSPList* hsp_list = Blast_HSPListNew(0);
       const int kNumHsps=25;
       for (int index=0; index<kNumHsps; index++)
       {
            BlastHSP* new_hsp;
            Blast_HSPInit(index, index+20, index+30, index+40, index+10, index+35,
                0, 0, 0, 10*index, NULL, &new_hsp);
            Blast_HSPListSaveHSP(hsp_list, new_hsp);
       }
       Blast_HSPListPurgeNullHSPs(hsp_list);
       /* Nothing was NULL on list, everything should be there. */
       BOOST_REQUIRE_EQUAL(kNumHsps, hsp_list->hspcnt);
       hsp_list = Blast_HSPListFree(hsp_list);
    }

    /* Check that a number of HSP's all with same score are saved correctly if there
     is a limit on the number of HSP's that may be saved. */
    BOOST_AUTO_TEST_CASE(testOrderingOfFullHSPList) {
       const int kNumHsps=5;
       const int kScore=10;
       BlastHSPList* hsp_list = Blast_HSPListNew(kNumHsps);
       BlastHSP* new_hsp;
       Blast_HSPInit(100, 100, 100, 100, 100, 100, 0, 0, 0, kScore, NULL, &new_hsp);
       Blast_HSPListSaveHSP(hsp_list, new_hsp);
       for (int index=0; index<kNumHsps; index++)
       {
            // ALL HSP's saved with score = 10
            Blast_HSPInit(index, index+20, index+30, index+40, index+10, index+35,
                0, 0, 0, kScore, NULL, &new_hsp);
            Blast_HSPListSaveHSP(hsp_list, new_hsp);
       }
       Blast_HSPInit(0, 19, 29, 40, 10, 35, 0, 0, 0, kScore, NULL, &new_hsp);
       Blast_HSPListSaveHSP(hsp_list, new_hsp);
       BlastHSP* hsp = hsp_list->hsp_array[0];
       BOOST_REQUIRE_EQUAL(3, hsp->query.offset);
       BOOST_REQUIRE_EQUAL(33, hsp->subject.offset);
       hsp = hsp_list->hsp_array[kNumHsps-1];
       BOOST_REQUIRE_EQUAL(0, hsp->query.offset);
       BOOST_REQUIRE_EQUAL(30, hsp->subject.offset);
       BOOST_REQUIRE_EQUAL(kNumHsps, hsp_list->hspcnt);
       hsp_list = Blast_HSPListFree(hsp_list);
    }

    /* Check that an HSPList with some missing HSP's is treated correctly. */
    BOOST_AUTO_TEST_CASE(testPurgeOfHSPListWithHoles) {
       BlastHSPList* hsp_list = Blast_HSPListNew(0);
       const int kNumHsps=25;
       for (int index=0; index<kNumHsps; index++)
       {    /* First we populate the hsp_list with HSP's. */
            BlastHSP* new_hsp;
            Blast_HSPInit(index, index+20, index+30, index+40, index+10, index+35,
                0, 0, 0, 10*index, NULL, &new_hsp);
            Blast_HSPListSaveHSP(hsp_list, new_hsp);
       }

       /* Now we "manually" free some of the HSP's. */
       hsp_list->hsp_array[0] = Blast_HSPFree(hsp_list->hsp_array[0]); // first
       hsp_list->hsp_array[7] = Blast_HSPFree(hsp_list->hsp_array[7]);
       hsp_list->hsp_array[17] = Blast_HSPFree(hsp_list->hsp_array[17]);
       hsp_list->hsp_array[24] = Blast_HSPFree(hsp_list->hsp_array[kNumHsps-1]); // last
       
       /* Purge, this should remove the "holes" in the array. */
       Blast_HSPListPurgeNullHSPs(hsp_list);

       BOOST_REQUIRE_EQUAL(kNumHsps-4, hsp_list->hspcnt);

       /* Check that what was in hsp_array[1] is now in hsp_array[0] */
       BOOST_REQUIRE_EQUAL(1, hsp_list->hsp_array[0]->query.offset);
       BOOST_REQUIRE_EQUAL(21, hsp_list->hsp_array[0]->query.end);

       /* Last four should now be NULLed out. */
       for (int index=kNumHsps-4; index<kNumHsps; index++)
       {
           BOOST_REQUIRE_EQUAL((BlastHSP*)0, hsp_list->hsp_array[index]);
       }
       
       hsp_list = Blast_HSPListFree(hsp_list);
    }

    /* Check that an HSP is correctly created by Blast_HSPInit. */
    BOOST_AUTO_TEST_CASE(testHSPInit) {
       const int kOffset=10;
       GapEditScript* edit_script = GapEditScriptNew(1);
       BlastHSP* new_hsp = NULL;

       GapEditScript * kPtrValue = edit_script;

       Int2 rv = Blast_HSPInit(kOffset, 2*kOffset, 3*kOffset, 4*kOffset, 
                               5*kOffset, 6*kOffset, 0, 0, 0, 10*kOffset, 
                               &edit_script, &new_hsp);
       BOOST_REQUIRE_EQUAL((Int2)0, rv);

       BOOST_REQUIRE_EQUAL((GapEditScript *) 0, edit_script); // this was NULL'ed out
       // HSP got the pointer to edit_script
       BOOST_REQUIRE_EQUAL(kPtrValue, new_hsp->gap_info); 
       BOOST_REQUIRE_EQUAL(kOffset, new_hsp->query.offset);
       BOOST_REQUIRE_EQUAL(2*kOffset, new_hsp->query.end);
       BOOST_REQUIRE_EQUAL(3*kOffset, new_hsp->subject.offset);
       BOOST_REQUIRE_EQUAL(4*kOffset, new_hsp->subject.end);
       BOOST_REQUIRE_EQUAL(10*kOffset, new_hsp->score);

       new_hsp = Blast_HSPFree(new_hsp);
       BOOST_REQUIRE(new_hsp == NULL);
    }

    BOOST_AUTO_TEST_CASE(testHSPInitWithNulls) {
       const int kOffset=10;
       BlastHSP* new_hsp = NULL;

       Int2 rv = Blast_HSPInit(kOffset, 2*kOffset, 3*kOffset, 4*kOffset, 5*kOffset, 6*kOffset,
                0, 0, 0, 10*kOffset, NULL, NULL);
       BOOST_REQUIRE_EQUAL((Int2)-1, rv);

       new_hsp = Blast_HSPFree(new_hsp);
       BOOST_REQUIRE(new_hsp == NULL);
    }

    static void 
    s_SetupQueryInfoForReevaluateTest(EBlastProgramType program_number,
                                      BLAST_SequenceBlk* query_blk,
                                      Uint4 subj_length,
                                      BlastQueryInfo* * query_info_ptr)
    {
        BlastQueryInfo* query_info = BlastQueryInfoNew(program_number, 1);
        
        query_info->contexts[0].eff_searchsp =
            (Int8) query_blk->length*subj_length;
        query_info->contexts[0].query_length = query_blk->length;
        
        /* mark the other contexts as invalid so they are not used. */
        for (int i=1; i<=query_info->last_context; i++)
            query_info->contexts[i].is_valid = false;
        
        *query_info_ptr = query_info;
    }

    static void 
    s_SetupScoringOptionsForReevaluateHSP(BlastScoringOptions* * options_ptr,
                                          bool gapped, bool is_prot)
    {
        BlastScoringOptions* options = 
            (BlastScoringOptions*) calloc(1, sizeof(BlastScoringOptions));
        if (gapped) {
           options->gapped_calculation = TRUE;
           options->gap_open = 1;
           options->gap_extend = 1;
        }
        if (is_prot) {
           options->matrix = strdup("BLOSUM62");
        } else {
           options->reward = 1;
           options->penalty = -2;
        }
        *options_ptr = options;
    }

    static void 
    s_SetupHSPForUngappedReevaluateNucl(BlastHSP* * hsp_ptr)
    {
        BlastHSP* hsp;
        
        if (*hsp_ptr != NULL) {
            hsp = *hsp_ptr;
        } else {
            *hsp_ptr = hsp = Blast_HSPNew();
        }

        hsp->query.offset = 0; 
        hsp->query.end = 20;
        hsp->subject.offset = 0;
        hsp->subject.end = 20;
        hsp->score = 20;
    }

    static void 
    s_SetupSequencesForUngappedReevaluateNucl(BLAST_SequenceBlk* * query_blk, 
                                            Uint1* * subject_seq_ptr)
    {
        const int kLength = 22;
        const Uint1 kQuerySeq[kLength+2] = { 15, 0, 1, 2, 3, 0, 0, 1, 1, 2, 2, 3,
                                            3, 3, 3, 0, 1, 2, 2, 1, 1, 0, 0, 15 };
        const Uint1 kSubjectSeq[kLength] = { 0, 1, 2, 2, 14, 14, 1, 1, 2, 2, 3, 
                                            3, 3, 3, 0, 1, 8, 2, 1, 1, 14, 0 };

        BlastSeqBlkNew(query_blk);
        BlastSeqBlkSetSequence(*query_blk, 
                               (Uint1*) BlastMemDup(kQuerySeq, kLength+2), 
                               kLength);
        *subject_seq_ptr = (Uint1*) BlastMemDup(kSubjectSeq, kLength);;
    }

    static void 
    checkReevaluateResultsUngappedNucl(BlastHSP* hsp)
    {
        const int kScore = 11;
        const int kQueryStart = 6;
        const int kQueryEnd = 20;
        const int kSubjectStart = 6;
        const int kSubjectEnd = 20;

        BOOST_REQUIRE_EQUAL(kScore, hsp->score);
        BOOST_REQUIRE_EQUAL(kQueryStart, hsp->query.offset);
        BOOST_REQUIRE_EQUAL(kQueryEnd, hsp->query.end);
        BOOST_REQUIRE_EQUAL(kSubjectStart, hsp->subject.offset);
        BOOST_REQUIRE_EQUAL(kSubjectEnd, hsp->subject.end);
    }

    /// Check reevaluation with ambiguities for a nucleotide ungapped search.
    BOOST_AUTO_TEST_CASE(testReevaluateWithAmbiguitiesUngappedNucl) {
        const int kWordCutoff = 3;
        const int kHitCutoff = 11;
        const double kEvalue = 1000;
        const Uint4 kSubjLength = 100000;
        const Uint4 kDbLength = 100000000;
        BlastHSP* hsp = NULL;
        BLAST_SequenceBlk* query_blk = NULL; 
        Uint1* subject_seq = NULL;
        BlastQueryInfo* query_info = NULL;
        EBlastProgramType program_number = eBlastTypeBlastn;
        BlastScoringOptions* scoring_options = NULL;
        BlastScoreBlk* sbp = NULL;
        BlastHitSavingOptions* hit_options = NULL;
        BlastHitSavingParameters* hit_params = NULL;
        BlastInitialWordOptions* word_options = NULL;
        BlastInitialWordParameters* word_params = NULL;

        s_SetupSequencesForUngappedReevaluateNucl(&query_blk, &subject_seq);
        s_SetupQueryInfoForReevaluateTest(program_number,
                                          query_blk, kDbLength, &query_info);

        s_SetupScoringOptionsForReevaluateHSP(&scoring_options, false, false);
        Blast_Message* blast_msg = NULL;
        BOOST_REQUIRE_EQUAL(0, 
            (int) BlastSetup_ScoreBlkInit(query_blk, query_info, 
                                           scoring_options, program_number, 
                                           &sbp, 1.0, &blast_msg,
                                           &BlastFindMatrixPath));

        BOOST_REQUIRE(blast_msg == NULL);
        BlastExtensionOptions* ext_options = NULL;
        BlastExtensionOptionsNew(program_number, &ext_options, 
                                 scoring_options->gapped_calculation);
        BlastHitSavingOptionsNew(program_number, &hit_options,
                                 scoring_options->gapped_calculation);
        hit_options->expect_value = kEvalue; 

        BlastHitSavingParametersNew(program_number, hit_options, sbp, query_info,
                                    kDbLength, &hit_params);

        BOOST_REQUIRE_EQUAL(kHitCutoff, hit_params->cutoff_score_min);

        QuerySetUpOptions* query_options = NULL;
        BlastQuerySetUpOptionsNew(&query_options);
        LookupTableWrap* lookup_wrap = NULL;
        LookupTableOptions* lookup_options = NULL; 
        BlastSeqLoc* blast_seq_loc = BlastSeqLocNew(NULL, 0, query_info->contexts[0].query_length-1);

        LookupTableOptionsNew(program_number, &lookup_options);
        LookupTableWrapInit(query_blk, lookup_options, query_options, blast_seq_loc, sbp, &lookup_wrap, NULL, NULL);
        query_options = BlastQuerySetUpOptionsFree(query_options);
        BlastInitialWordOptionsNew(program_number, &word_options);
        BlastInitialWordParametersNew(program_number, word_options, hit_params, 
              lookup_wrap, sbp, query_info, kSubjLength, &word_params);

        blast_seq_loc = BlastSeqLocFree(blast_seq_loc);
        lookup_wrap = LookupTableWrapFree(lookup_wrap);
        lookup_options = LookupTableOptionsFree(lookup_options);

        word_params->cutoff_score_min = kWordCutoff;

        s_SetupHSPForUngappedReevaluateNucl(&hsp);

        BOOST_REQUIRE(Blast_HSPReevaluateWithAmbiguitiesUngapped(hsp, 
                           query_blk->sequence, subject_seq, word_params, 
                           sbp, FALSE) == FALSE);

        checkReevaluateResultsUngappedNucl(hsp);

        Blast_HSPFree(hsp);
        BlastSequenceBlkFree(query_blk);
        sfree(subject_seq);
        BlastInitialWordParametersFree(word_params);
        BlastExtensionOptionsFree(ext_options);
        BlastHitSavingParametersFree(hit_params);
        BlastInitialWordOptionsFree(word_options);
        BlastHitSavingOptionsFree(hit_options);
        BlastScoringOptionsFree(scoring_options);
        BlastQueryInfoFree(query_info);
        BlastScoreBlkFree(sbp);
    }

     static void 
    s_SetupHSPListForUngappedReevaluateTransl(BlastHSPList* * hsplist_ptr)
    {
        BlastHSP* hsp = Blast_HSPNew();

        hsp->query.offset = 0; 
        hsp->query.end = 12;
        hsp->subject.offset = 0;
        hsp->subject.frame = 1;
        hsp->subject.end = 12;
        hsp->score = 45;
        
        *hsplist_ptr = Blast_HSPListNew(1);
        (*hsplist_ptr)->hsp_array[0] = hsp;
        (*hsplist_ptr)->hspcnt = 1;
    }

    static void 
    s_SetupSequencesForUngappedReevaluateTransl(BLAST_SequenceBlk* * query_blk, 
                                                BLAST_SequenceBlk* * subject_blk)
    {
        const int kLength = 12;
        const Uint1 kQuerySeq[kLength] = 
           { 11, 19, 13, 1, 9, 22, 6, 10, 7, 12, 20, 10 };
        const Uint1 kSubjectSeq[3*kLength] = 
           { 2, 8, 4, 4, 8, 4, 1, 1, 10, 8, 15, 2, 1, 8, 1, 8, 8, 15, 8, 8, 8,
             1, 5, 1, 4, 4, 2, 1, 1, 1, 8, 4, 4, 4, 1, 4 };

        BlastSeqBlkNew(query_blk);
        BlastSeqBlkSetSequence(*query_blk, 
                               (Uint1*) BlastMemDup(kQuerySeq, kLength), 
                               kLength);
        (*query_blk)->sequence = (*query_blk)->sequence_start;
        BlastSeqBlkNew(subject_blk);
        BlastSeqBlkSetSequence(*subject_blk, 
                               (Uint1*) BlastMemDup(kSubjectSeq, 3*kLength), 
                               3*kLength);
        (*subject_blk)->sequence = (*subject_blk)->sequence_start;
    }

    static void 
    checkReevaluateResultsUngappedTransl(BlastHSPList* hsp_list)
    {
        const int kScore = 38;
        const int kQueryStart = 0;
        const int kQueryEnd = 12;
        const int kSubjectStart = 0;
        const int kSubjectEnd = 12;
        BlastHSP* hsp = hsp_list->hsp_array[0];

        BOOST_REQUIRE(hsp);
        BOOST_REQUIRE_EQUAL(kScore, hsp->score);
        BOOST_REQUIRE_EQUAL(kQueryStart, hsp->query.offset);
        BOOST_REQUIRE_EQUAL(kQueryEnd, hsp->query.end);
        BOOST_REQUIRE_EQUAL(kSubjectStart, hsp->subject.offset);
        BOOST_REQUIRE_EQUAL(kSubjectEnd, hsp->subject.end);
    }

    /// Check reevaluation with ambiguities for a translated ungapped search.
    /// Also checks the identity and length test.
    BOOST_AUTO_TEST_CASE(testReevaluateWithAmbiguitiesUngappedTransl)
    {
        const int kWordCutoff = 37;
        const int kHitCutoff = 51;
        const Uint4 kSubjLength = 50000;
        const Uint4 kDbLength = 100000000;
        BlastHSPList* hsp_list = NULL;
        BLAST_SequenceBlk* query_blk = NULL; 
        BLAST_SequenceBlk* subject_blk = NULL; 
        BlastQueryInfo* query_info = NULL;
        EBlastProgramType program_number = eBlastTypeTblastn;
        BlastScoringOptions* scoring_options = NULL;
        BlastScoreBlk* sbp = NULL;
        BlastHitSavingOptions* hit_options = NULL;
        BlastHitSavingParameters* hit_params = NULL;
        BlastInitialWordOptions* word_options = NULL;
        BlastInitialWordParameters* word_params = NULL;

        s_SetupSequencesForUngappedReevaluateTransl(&query_blk, &subject_blk);
        s_SetupQueryInfoForReevaluateTest(program_number,
                                          query_blk, kDbLength, &query_info);

        s_SetupScoringOptionsForReevaluateHSP(&scoring_options, false, true);
        BlastScoringParameters* score_params = 
           (BlastScoringParameters*) calloc(1, sizeof(BlastScoringParameters));
        score_params->options = scoring_options;
        Blast_Message* blast_msg = NULL;

        BOOST_REQUIRE_EQUAL(0, 
            (int) BlastSetup_ScoreBlkInit(query_blk, query_info, 
                                           scoring_options, program_number, 
                                           &sbp, 1.0, &blast_msg,
                                           &BlastFindMatrixPath));

        BlastExtensionOptions* ext_options = NULL;
        BlastExtensionOptionsNew(program_number, &ext_options, 
                                 scoring_options->gapped_calculation);
        BlastHitSavingOptionsNew(program_number, &hit_options,
                                 scoring_options->gapped_calculation);

        BlastHitSavingParametersNew(program_number, hit_options, sbp, query_info,
                                    kDbLength, &hit_params);

        BOOST_REQUIRE_EQUAL(kHitCutoff, hit_params->cutoff_score_min);

        QuerySetUpOptions* query_options = NULL;
        BlastQuerySetUpOptionsNew(&query_options);
        LookupTableWrap* lookup_wrap = NULL;
        LookupTableOptions* lookup_options = NULL; 
        BlastSeqLoc* blast_seq_loc = BlastSeqLocNew(NULL, 0, query_info->contexts[0].query_length-1);

        LookupTableOptionsNew(program_number, &lookup_options);
        LookupTableWrapInit(query_blk, lookup_options, query_options, blast_seq_loc, sbp, &lookup_wrap, NULL, NULL);
        query_options = BlastQuerySetUpOptionsFree(query_options);
        BlastInitialWordOptionsNew(program_number, &word_options);
        BlastInitialWordParametersNew(program_number, word_options, hit_params, 
              lookup_wrap, sbp, query_info, kSubjLength, &word_params);

        blast_seq_loc = BlastSeqLocFree(blast_seq_loc);
        lookup_wrap = LookupTableWrapFree(lookup_wrap);
        lookup_options = LookupTableOptionsFree(lookup_options);

        BOOST_REQUIRE_EQUAL(kWordCutoff, word_params->cutoff_score_min);

        s_SetupHSPListForUngappedReevaluateTransl(&hsp_list);

        Uint1* gen_code_string = (Uint1*)
           BlastMemDup(FindGeneticCode(1).get(), 64);

        Blast_HSPListReevaluateUngapped(program_number, 
                                 hsp_list, query_blk, subject_blk, 
                                 word_params, hit_params, query_info, sbp, 
                                 score_params, NULL, gen_code_string);

        checkReevaluateResultsUngappedTransl(hsp_list);

        // Now check identity and length test: the percent identity for the 
        // first HSP is below 80%.
        hit_params->options->percent_identity = 80;
        BOOST_REQUIRE(Blast_HSPTestIdentityAndLength(program_number, 
                           hsp_list->hsp_array[0], query_blk->sequence, 
                           subject_blk->sequence, score_params->options, 
                           hit_params->options));

        sfree(gen_code_string);
        Blast_HSPListFree(hsp_list);
        BlastSequenceBlkFree(query_blk);
        BlastSequenceBlkFree(subject_blk);
        BlastInitialWordParametersFree(word_params);
        BlastHitSavingParametersFree(hit_params);
        BlastInitialWordOptionsFree(word_options);
        BlastExtensionOptionsFree(ext_options);
        BlastHitSavingOptionsFree(hit_options);
        BlastScoringParametersFree(score_params);
        BlastScoringOptionsFree(scoring_options);
        BlastQueryInfoFree(query_info);
        BlastScoreBlkFree(sbp);
    }

   static void 
    s_SetupHSPForGappedReevaluateTest(BlastHSP* * hsp_ptr)
    {
        const int kNumSegs = 5;
        const EGapAlignOpType kEditScriptOpType[kNumSegs] = 
            { eGapAlignSub, eGapAlignDel, eGapAlignSub, eGapAlignDel, 
              eGapAlignSub };
        const Int4 kEditScriptNum[kNumSegs] = { 4, 1, 11, 1, 5 };
        int index;
        GapEditScript* esp = NULL;
        BlastHSP* hsp;
        
        if (*hsp_ptr != NULL) {
            hsp = *hsp_ptr;
        } else {
            *hsp_ptr = hsp = Blast_HSPNew();
        }

        hsp->query.offset = 0; 
        hsp->query.end = 20;
        hsp->subject.offset = 2;
        hsp->subject.end = 24;
        hsp->score = 13;

        if (hsp->gap_info)
            hsp->gap_info = GapEditScriptDelete(hsp->gap_info);

        esp = GapEditScriptNew(kNumSegs);
        hsp->gap_info = esp;
        for (index = 0; index < kNumSegs; ++index) {
            esp->op_type[index] = kEditScriptOpType[index];
            esp->num[index] = kEditScriptNum[index];
        }
    }

    static void 
    s_SetupSequencesForGappedReevaluateTest(BLAST_SequenceBlk* * query_blk, 
                                          Uint1* * subject_seq_ptr, 
                                          Uint4* subj_length)
    {
        const int kQueryLength = 20;
        const int kSubjectLength = 25;
        const Uint1 kQuerySeq[kQueryLength+2] = { 15, 0, 1, 2, 3, 0, 0, 1, 1, 
                                                  2, 2, 3, 3, 3, 3, 2, 2, 1, 1, 
                                                  0, 0, 15 };
        const Uint1 kSubjectSeq[kSubjectLength] = { 1, 2, 0, 1, 2, 3, 3, 2, 
                                                    14, 1, 1, 2, 2, 3, 3, 3, 3, 
                                                    2, 0, 2, 1, 14, 0, 0, 3 };

        BlastSeqBlkNew(query_blk);
        BlastSeqBlkSetSequence(*query_blk, 
                               (Uint1*) BlastMemDup(kQuerySeq, kQueryLength+2), 
                               kQueryLength);
        *subj_length = kSubjectLength;
        *subject_seq_ptr = (Uint1*) BlastMemDup(kSubjectSeq, kSubjectLength);;
    }

    static void 
    checkReevaluateResultsGapped(BlastHSP* hsp)
    {
        const int kScore = 10;
        const int kQueryStart = 6;
        const int kQueryEnd = 20;
        const int kSubjectStart = 9;
        const int kSubjectEnd = 24;
        const int kNumSegs = 3;
        const Uint1 kEditScriptOpType[kNumSegs] = 
            { eGapAlignSub, eGapAlignDel, eGapAlignSub };
        const Uint1 kEditScriptNum[kNumSegs] = { 9, 1, 5 };

        BOOST_REQUIRE_EQUAL(kScore, hsp->score);
        BOOST_REQUIRE_EQUAL(kQueryStart, hsp->query.offset);
        BOOST_REQUIRE_EQUAL(kQueryEnd, hsp->query.end);
        BOOST_REQUIRE_EQUAL(kSubjectStart, hsp->subject.offset);
        BOOST_REQUIRE_EQUAL(kSubjectEnd, hsp->subject.end);

        int index;
        GapEditScript* esp = NULL;

        for (index = 0, esp = hsp->gap_info; index < esp->size; ++index) {
            BOOST_REQUIRE_EQUAL((int)kEditScriptOpType[index], (int)esp->op_type[index]);
            BOOST_REQUIRE_EQUAL((int)kEditScriptNum[index], (int)esp->num[index]);
        }
        BOOST_REQUIRE_EQUAL(kNumSegs, index);
    }

    /// Check the gapped version of reevaluation with ambiguities.
    BOOST_AUTO_TEST_CASE(testReevaluateWithAmbiguitiesGapped)
    {
        BlastHSP* hsp = NULL;
        BLAST_SequenceBlk* query_blk = NULL; 
        Uint1* subject_seq = NULL;
        BlastQueryInfo* query_info = NULL;
        BlastHitSavingOptions* hit_options = NULL;
        BlastScoringOptions* scoring_options = NULL;
        BlastScoringParameters* scoring_params = NULL;
        BlastScoreBlk* sbp = NULL;
        Blast_Message* blast_message = NULL;
        EBlastProgramType program_number = eBlastTypeBlastn;
        Uint4 subj_length = 0;

        s_SetupSequencesForGappedReevaluateTest(&query_blk, &subject_seq, 
                                              &subj_length);
        s_SetupQueryInfoForReevaluateTest(program_number,
                                          query_blk, subj_length, &query_info);
        s_SetupScoringOptionsForReevaluateHSP(&scoring_options, true, false);

        BlastExtensionOptions* ext_options = NULL;
        BlastExtensionOptionsNew(program_number, &ext_options, 
                                 scoring_options->gapped_calculation);
        BlastHitSavingOptionsNew(program_number, &hit_options,
                                 scoring_options->gapped_calculation);
        hit_options->expect_value = 1; 
        BOOST_REQUIRE_EQUAL(0, 
            (int) BlastSetup_ScoreBlkInit(query_blk, query_info, 
                                           scoring_options, program_number, 
                                           &sbp, 1.0, &blast_message,
                                           &BlastFindMatrixPath));
        BOOST_REQUIRE_EQUAL(0, 
            (int) BlastScoringParametersNew(scoring_options, sbp,
                                            &scoring_params));

        BlastHitSavingParameters* hit_params = (BlastHitSavingParameters*) 
            calloc(1, sizeof(BlastHitSavingParameters));

        BOOST_REQUIRE(query_blk);
        BOOST_REQUIRE(sbp->kbp_gap[0]);

        BLAST_Cutoffs(&hit_params->cutoff_score_min, 
                      &hit_options->expect_value, 
                      sbp->kbp_gap[0], subj_length*query_blk->length, FALSE, 0);
        hit_params->options = hit_options;
        hit_params->cutoffs = (BlastGappedCutoffs *)calloc(1,
                                                  sizeof(BlastGappedCutoffs));
        hit_params->cutoffs[0].cutoff_score = hit_params->cutoff_score_min;

        s_SetupHSPForGappedReevaluateTest(&hsp);
        Blast_HSPReevaluateWithAmbiguitiesGapped(hsp, query_blk->sequence, 
            subject_seq, hit_params, scoring_params, sbp);

        // With e-value 1, the low-scoring front piece of the alignment is 
        // cut off.
        checkReevaluateResultsGapped(hsp);
        
        hit_options->expect_value = 10;
        hit_params->cutoff_score_min = 1;
        BLAST_Cutoffs(&hit_params->cutoff_score_min, 
                      &hit_options->expect_value, 
                      sbp->kbp_gap[0], subj_length*query_blk->length, FALSE, 0);
        hit_params->cutoffs[0].cutoff_score = hit_params->cutoff_score_min;
        
        s_SetupHSPForGappedReevaluateTest(&hsp);
        Blast_HSPReevaluateWithAmbiguitiesGapped(hsp, query_blk->sequence, 
            subject_seq, hit_params, scoring_params, sbp);
        // With e-value 10, the front piece of the alignment is left, and the 
        // remainder is cut off.
        checkReevaluateResultsGapped(hsp);

        // Now check identity and length test: the percent identity for the 
        // First HSP identity is above 80%.
        hit_params->options->percent_identity = 80;
        BOOST_REQUIRE_EQUAL(0, 
            (int) Blast_HSPTestIdentityAndLength(program_number, 
                      hsp, query_blk->sequence, subject_seq,
                      scoring_params->options, hit_params->options));
        // But its length is less than 20.
        hit_params->options->min_hit_length = 20;
        BOOST_REQUIRE_EQUAL(1, 
            (int) Blast_HSPTestIdentityAndLength(program_number, 
                      hsp, query_blk->sequence, subject_seq,
                      scoring_params->options, hit_params->options));

        Blast_HSPFree(hsp);
        BlastSequenceBlkFree(query_blk);
        sfree(subject_seq);
        BlastExtensionOptionsFree(ext_options);
        BlastHitSavingOptionsFree(hit_options);
        BlastHitSavingParametersFree(hit_params);
        BlastScoringOptionsFree(scoring_options);
        BlastScoringParametersFree(scoring_params);
        BlastQueryInfoFree(query_info);
        BlastScoreBlkFree(sbp);
        Blast_MessageFree(blast_message);
    }

    BOOST_AUTO_TEST_CASE(testReevaluateWithAmbiguitiesBadHit)
    {
        const EBlastProgramType kProgram = eBlastTypeBlastn;
        const int kLength = 22;
        const int kGappedStart = 1;
        const int kCutoff = 4;
        Uint1 query[kLength] = { 2, 0, 1, 3, 2, 0, 2, 1, 2, 0, 1, 3, 3, 1, 0, 1, 
                                 3, 3, 3, 1, 0, 1 };
        Uint1 subject[kLength] = { 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
                                   14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14 };
        BlastHSP* hsp = (BlastHSP*) calloc(1, sizeof(BlastHSP));
        hsp->query.end = hsp->subject.end = kLength;
        hsp->query.gapped_start = hsp->subject.gapped_start = kGappedStart;
        hsp->gap_info = GapEditScriptNew(1);
        hsp->gap_info->op_type[0] = eGapAlignSub;
        hsp->gap_info->num[0] = kLength;
     
        BlastScoringOptions* score_opts = NULL;
        BlastScoringOptionsNew(kProgram, &score_opts);
        BlastScoreBlk* sbp = BlastScoreBlkNew(BLASTNA_SEQ_CODE, 1);
        Blast_ScoreBlkMatrixInit(kProgram, score_opts, sbp, NULL);
        BlastScoringParameters* score_params = NULL;
        BlastScoringParametersNew(score_opts, sbp, &score_params);
        BlastHitSavingOptions* hit_opts = NULL;
        BlastExtensionOptions* ext_options = NULL;
        BlastExtensionOptionsNew(kProgram, &ext_options, 
                                 score_opts->gapped_calculation);
        BlastHitSavingOptionsNew(kProgram, &hit_opts,
                                 score_opts->gapped_calculation);
        BlastHitSavingParameters* hit_params = (BlastHitSavingParameters*)
            calloc(1, sizeof(BlastHitSavingParameters));
        hit_params->options = hit_opts;
        hit_params->cutoff_score_min = kCutoff;
        hit_params->cutoffs = (BlastGappedCutoffs *)calloc(1,
                                                  sizeof(BlastGappedCutoffs));
        hit_params->cutoffs[0].cutoff_score = hit_params->cutoff_score_min;

        Boolean delete_hsp = 
            Blast_HSPReevaluateWithAmbiguitiesGapped(hsp, query, subject, 
                                                     hit_params, score_params,
                                                     sbp);
        BOOST_REQUIRE(delete_hsp == TRUE);

        hsp = Blast_HSPFree(hsp);
        sbp = BlastScoreBlkFree(sbp);
        score_params = BlastScoringParametersFree(score_params);
        ext_options = BlastExtensionOptionsFree(ext_options);
        score_opts = BlastScoringOptionsFree(score_opts);
        hit_params = BlastHitSavingParametersFree(hit_params);
        hit_opts = BlastHitSavingOptionsFree(hit_opts);
    }

    BOOST_AUTO_TEST_CASE(testGetOOFNumIdentities)
    {
        const char* query = "ADADADADADBADADADADADADADAADADAD";
        const char* subject = 
            "ABCDBCABCDBCEABCDBCABCDBCABCDBCABCDBCABCDBABCDBCABCDABCDBCABCDBCFFABCDBCABCDBCABCDBCABCDBCGBCDBC";
        const int kNumSegs = 13;
        const EGapAlignOpType kEditScriptOp[kNumSegs] = 
            { eGapAlignSub, eGapAlignIns1, eGapAlignSub, eGapAlignDel, 
              eGapAlignSub, eGapAlignDel1, eGapAlignSub, eGapAlignDel2,
              eGapAlignSub, eGapAlignIns2, eGapAlignSub, eGapAlignIns, 
              eGapAlignSub };
        const int kEditScriptNum[kNumSegs] = { 4, 1, 6, 1, 4, 1, 4, 1, 4, 1, 4, 1, 6 };
        const int kGoodNumIdent = 30;
        const int kGoodAlignLength = 34;
        int index;
        Int4 num_ident = 0, align_length = 0;

        BlastHSP* hsp = Blast_HSPNew();
        hsp->query.offset = 0; 
        hsp->query.end = 32;
        hsp->subject.offset = 0;
        hsp->subject.end = 96;

        GapEditScript* esp = GapEditScriptNew(kNumSegs);
        hsp->gap_info = esp;
        for (index = 0; index < kNumSegs; ++index) {
            esp->op_type[index] = kEditScriptOp[index];
            esp->num[index] = kEditScriptNum[index];
        }

        BlastScoringOptions* scoring_opts = NULL;
        BlastScoringOptionsNew(eBlastTypeTblastn, &scoring_opts);
        scoring_opts->is_ooframe = TRUE;

        Blast_HSPGetNumIdentities((Uint1*)query, (Uint1*)subject, hsp, 
                                  scoring_opts, &align_length);
        num_ident = hsp->num_ident;
        Blast_HSPFree(hsp);
        BlastScoringOptionsFree(scoring_opts);
        BOOST_REQUIRE_EQUAL(kGoodNumIdent, num_ident);
        BOOST_REQUIRE_EQUAL(kGoodAlignLength, align_length);
    }

   BOOST_AUTO_TEST_CASE(testHSPListSort) 
   {
      const int kHspCount = 10;
      const int kScores[kHspCount] = 
	 { 10, 20, 15, 100, 21, 40, 55, 30, 90, 40 };
      const double kEvalues[kHspCount] = 
	 { 1, 0.1, 0.5, 0, 0.11, 0.01, 0.001, 0.05, 0, 0.01 };

      const int kScoresSorted[kHspCount] = 
	 { 100, 90, 55, 40, 40, 30, 21, 20, 15, 10 };
      const double kEvaluesSorted[kHspCount] = 
	 { 0, 0, 0.001, 0.01, 0.01, 0.05, 0.1, 0.11, 0.5, 1 };

      BlastHSPList* hsp_list = Blast_HSPListNew(kHspCount);
      int index;

      hsp_list->hspcnt = kHspCount;

      for (index = 0; index < kHspCount; ++index) {
	 hsp_list->hsp_array[index] = Blast_HSPNew();
	 hsp_list->hsp_array[index]->query.offset = index;
	 hsp_list->hsp_array[index]->query.end = index + kScores[index];
	 hsp_list->hsp_array[index]->subject.offset = 10 - index;
	 hsp_list->hsp_array[index]->subject.end = 
	    10 - index + kScores[index];
	 hsp_list->hsp_array[index]->score = kScores[index];
	 hsp_list->hsp_array[index]->evalue = kEvalues[index];
      }

      Blast_HSPListSortByScore(hsp_list);
      for (index = 0; index < kHspCount; ++index) {
	 BOOST_REQUIRE_EQUAL(kScoresSorted[index], 
			      hsp_list->hsp_array[index]->score);
      }
      Blast_HSPListSortByEvalue(hsp_list);
      for (index = 0; index < kHspCount; ++index) {
	 BOOST_REQUIRE_EQUAL(kEvaluesSorted[index], 
			      hsp_list->hsp_array[index]->evalue);
      }
      /* Check that subject offset tie breaker was correctly applied
	 between HSP's #5 and #9. */
      BOOST_REQUIRE_EQUAL(5, hsp_list->hsp_array[4]->subject.offset); 

      Blast_HSPListFree(hsp_list);
   }

    static void s_AddNextHSP(BlastHSPList* hsp_list, int& score)
    {
        BlastHSP* hsp = Blast_HSPNew();
        if (score == 0) 
            score = 19;
        hsp->score = score = (31 * score) % 100;
        Blast_HSPListSaveHSP(hsp_list, hsp);
    }

    /// Tests how HSPs are saved, including the HSP limit.
    BOOST_AUTO_TEST_CASE(testHSPListSaveHSP) {
        const int kHspNumMax = 250;
        const int kTotal = 1000;
        const int kMaxScore = 99;
        int index;
        int score = 0;
        

        BlastHSPList* hsp_list = Blast_HSPListNew(kHspNumMax);
        BOOST_REQUIRE_EQUAL(100, hsp_list->allocated);

        for (index = 0; index < kHspNumMax; ++index) {
            s_AddNextHSP(hsp_list, score);
        }
        BOOST_REQUIRE_EQUAL(hsp_list->hspcnt, kHspNumMax);
        BOOST_REQUIRE_EQUAL(hsp_list->allocated, kHspNumMax);
        BOOST_REQUIRE(hsp_list->do_not_reallocate == FALSE);

        s_AddNextHSP(hsp_list, score);
        BOOST_REQUIRE_EQUAL(hsp_list->hspcnt, kHspNumMax);
        BOOST_REQUIRE_EQUAL(hsp_list->allocated, kHspNumMax);
        // The flag prohibiting further reallocation should have been set.
        BOOST_REQUIRE(hsp_list->do_not_reallocate == TRUE);
        // Check that HSP array has been heapified
        int index1;
        for (index1 = 1; index1 < kHspNumMax; ++index1) {
            BOOST_REQUIRE(hsp_list->hsp_array[(index1-1)/2]->score <=
                           hsp_list->hsp_array[index1]->score);
        }

        for (++index; index < kTotal; ++index) {
            s_AddNextHSP(hsp_list, score);
        }
        // Check that HSP array is still heapified
        for (index = 1, index1 = 0; index < kHspNumMax; ++index) {
            BOOST_REQUIRE(hsp_list->hsp_array[(index-1)/2]->score <=
                           hsp_list->hsp_array[index]->score);
            if (hsp_list->hsp_array[index]->score == kMaxScore)
                ++index1;
        }
        // Check the first and last score in the array
        BOOST_REQUIRE_EQUAL(79, hsp_list->hsp_array[0]->score);
        BOOST_REQUIRE_EQUAL(89, hsp_list->hsp_array[kHspNumMax-1]->score);
        // Check that correct number of HSPs have maximal score
        BOOST_REQUIRE_EQUAL(100, index1);

        Blast_HSPListFree(hsp_list);
    }

    BOOST_AUTO_TEST_CASE(testCheckHSPCommonEndpoints) {
        const int kHspCountStart = 9;
        const int kHspCountEnd = 3;
        const int kScores[kHspCountStart] = 
            { 1044, 995, 965, 219, 160, 125, 110, 107, 103 };
        const int kQueryOffsets[kHspCountStart] = 
            { 2, 2, 2, 236, 88, 259, 278, 259, 278 };
        const int kQueryEnds[kHspCountStart] = 
            { 322, 336, 300, 322, 182, 322, 341, 341, 341 };
        const int kSubjectOffsets[kHspCountStart] = 
            { 7, 7, 7, 194, 2, 194, 197, 194, 197 };
        const int kSubjectEnds[kHspCountStart] = 
            { 292, 293, 301, 292, 96, 292, 260, 260, 266 };
        const int kSurvivingIndices[kHspCountEnd] = { 4, 0, 6 };
        EBlastProgramType program;

        BlastHSPList* hsp_list = Blast_HSPListNew(0);

        for (int index = 0; index < kHspCountStart; ++index) {
            BlastHSP* hsp;
            Blast_HSPInit(kQueryOffsets[index], kQueryEnds[index],
                  kSubjectOffsets[index], kSubjectEnds[index],
                  0, 0, 
                  0, 0, 0,
                  kScores[index], NULL, &hsp);
            Blast_HSPListSaveHSP(hsp_list, hsp);
            hsp = NULL;
        }

        // Check first that if program is set to PHI BLAST, nothing will be done.
        program = eBlastTypePhiBlastp;
        BOOST_REQUIRE_EQUAL(kHspCountStart, 
                             Blast_HSPListPurgeHSPsWithCommonEndpoints(program, 
                                                                       hsp_list));

        // Now check that for a non-PHI program the routine works properly.
        program = eBlastTypeBlastp;
        BOOST_REQUIRE_EQUAL(kHspCountEnd, 
                             Blast_HSPListPurgeHSPsWithCommonEndpoints(program, 
                                                                       hsp_list));

        BlastHSP** hsp_array = hsp_list->hsp_array;
        for (int index = 0; index < kHspCountEnd; ++index) {
            int index_orig = kSurvivingIndices[index];
            BOOST_REQUIRE_EQUAL(kScores[index_orig], hsp_array[index]->score);
            BOOST_REQUIRE_EQUAL(kQueryOffsets[index_orig], 
                                 hsp_array[index]->query.offset);
            BOOST_REQUIRE_EQUAL(kSubjectOffsets[index_orig], 
                                 hsp_array[index]->subject.offset);
            BOOST_REQUIRE_EQUAL(kQueryEnds[index_orig], 
                                 hsp_array[index]->query.end);
            BOOST_REQUIRE_EQUAL(kSubjectEnds[index_orig], 
                                 hsp_array[index]->subject.end);
            sfree(hsp_array[index]);
        }
        
        hsp_list = Blast_HSPListFree(hsp_list);
   }

   BOOST_AUTO_TEST_CASE(testSBlastHitsParamsGapped) {

        const EBlastProgramType kProgram = eBlastTypeBlastp;

        BlastScoringOptions* scoring_options = NULL;
        BlastScoringOptionsNew(kProgram, &scoring_options);

        BlastExtensionOptions* ext_options = NULL;
        BlastExtensionOptionsNew(kProgram, &ext_options, 
                                 scoring_options->gapped_calculation);

        BlastHitSavingOptions* hit_options = NULL;
        BlastHitSavingOptionsNew(kProgram, &hit_options,
                                 scoring_options->gapped_calculation);

        SBlastHitsParameters* blasthit_params=NULL;
        SBlastHitsParametersNew(hit_options, ext_options, scoring_options,
                                &blasthit_params);

        scoring_options = BlastScoringOptionsFree(scoring_options);
        ext_options = BlastExtensionOptionsFree(ext_options);

        BOOST_REQUIRE_EQUAL(blasthit_params->prelim_hitlist_size, 
             hit_options->hitlist_size+50);

        ext_options = BlastExtensionOptionsFree(ext_options);
        blasthit_params = SBlastHitsParametersFree(blasthit_params);
        hit_options = BlastHitSavingOptionsFree(hit_options);
   }

   BOOST_AUTO_TEST_CASE(testSBlastHitsParamsUngapped) {

        const EBlastProgramType kProgram = eBlastTypeBlastp;

        BlastScoringOptions* scoring_options = NULL;
        BlastScoringOptionsNew(kProgram, &scoring_options);
        scoring_options->gapped_calculation = FALSE;

        BlastExtensionOptions* ext_options = NULL;
        BlastExtensionOptionsNew(kProgram, &ext_options, 
                                 scoring_options->gapped_calculation);

        BlastHitSavingOptions* hit_options = NULL;
        BlastHitSavingOptionsNew(kProgram, &hit_options,
                                 scoring_options->gapped_calculation);

        SBlastHitsParameters* blasthit_params=NULL;
        SBlastHitsParametersNew(hit_options, ext_options, scoring_options, 
                                &blasthit_params);

        scoring_options = BlastScoringOptionsFree(scoring_options);
        ext_options = BlastExtensionOptionsFree(ext_options);

        BOOST_REQUIRE_EQUAL(blasthit_params->prelim_hitlist_size, 
             hit_options->hitlist_size);

        ext_options = BlastExtensionOptionsFree(ext_options);
        blasthit_params = SBlastHitsParametersFree(blasthit_params);
        hit_options = BlastHitSavingOptionsFree(hit_options);
   }

   BOOST_AUTO_TEST_CASE(testSBlastHitsParamsCompStats) {

        const EBlastProgramType kProgram = eBlastTypeBlastp;

        BlastScoringOptions* scoring_options = NULL;
        BlastScoringOptionsNew(kProgram, &scoring_options);

        BlastExtensionOptions* ext_options = NULL;
        BlastExtensionOptionsNew(kProgram, &ext_options, 
                                 scoring_options->gapped_calculation);

        BlastHitSavingOptions* hit_options = NULL;
        BlastHitSavingOptionsNew(kProgram, &hit_options,
                                 scoring_options->gapped_calculation);

        BlastExtensionOptionsNew(kProgram, &ext_options, scoring_options->gapped_calculation);
        ext_options->compositionBasedStats = TRUE;

        SBlastHitsParameters* blasthit_params=NULL;
        SBlastHitsParametersNew(hit_options, ext_options, scoring_options,
                                &blasthit_params);

        scoring_options = BlastScoringOptionsFree(scoring_options);
        ext_options = BlastExtensionOptionsFree(ext_options);

        BOOST_REQUIRE_EQUAL(blasthit_params->prelim_hitlist_size, 
             (2*hit_options->hitlist_size)+50);

        ext_options = BlastExtensionOptionsFree(ext_options);
        blasthit_params = SBlastHitsParametersFree(blasthit_params);
        hit_options = BlastHitSavingOptionsFree(hit_options);
    }

    BOOST_AUTO_TEST_CASE(testAdjustOddBlastnScores) {
        const int kHspCnt = 10;
        const bool kGapped = TRUE;
        

        BlastHSPList* hsp_list = Blast_HSPListNew(kHspCnt);
        BlastScoreBlk sb;
        // sb.kbp_gap = (Blast_KarlinBlk**) calloc(1, sizeof(Blast_KarlinBlk*));
        // sb.kbp_gap[0] = (Blast_KarlinBlk*) calloc(1, sizeof(Blast_KarlinBlk));
        sb.round_down = true;
        
        // Check that NULL input or hsp_list with no HSPs do not cause trouble.
        Blast_HSPListAdjustOddBlastnScores(NULL, kGapped, &sb);
        Blast_HSPListAdjustOddBlastnScores(hsp_list, kGapped, &sb);

        // Set up an HSP list.
        for (int index = 0; index < kHspCnt; ++index) {
            BlastHSP* hsp = Blast_HSPNew();
            hsp->query.offset = 100*index;
            hsp->query.end = hsp->query.offset - 5*index + 100;
            hsp->subject.offset = 2000 - 100*index;
            hsp->subject.end = hsp->subject.offset - 5*index + 100;
            hsp->score = 100 - index;
            hsp_list->hsp_array[index] = hsp;
        }
        hsp_list->hspcnt = kHspCnt;

        Blast_HSPListAdjustOddBlastnScores(hsp_list, kGapped, &sb);
        BOOST_REQUIRE(Blast_HSPListIsSortedByScore(hsp_list) == TRUE);
        for (int index = 0; index < kHspCnt; ++index)
            BOOST_REQUIRE((hsp_list->hsp_array[index]->score & 1) == 0);
        Blast_HSPListFree(hsp_list);
        // sfree(sb.kbp_gap[0]);
        // sfree(sb.kbp_gap);
    }

    BOOST_AUTO_TEST_CASE(testHitListUpdate) {
        const int kBig = kMax_I4/2; // about 1 billion
        BlastHitList* hit_list = Blast_HitListNew(kBig);
        BlastHSPList* hsp_list = Blast_HSPListNew(0);
        setupHSPList(&hsp_list, 0);

        Int2 status = Blast_HitListUpdate(hit_list, hsp_list);
        BOOST_REQUIRE_EQUAL(0, (int) status);
        BOOST_REQUIRE_EQUAL(1, (int) hit_list->hsplist_count);
        BOOST_REQUIRE_EQUAL(100, (int) hit_list->hsplist_current);
        BOOST_REQUIRE_EQUAL(kBig, (int) hit_list->hsplist_max);
        BOOST_REQUIRE_EQUAL(60, (int) hit_list->low_score);
        BOOST_REQUIRE(hit_list->hsplist_array != NULL);
        hit_list = Blast_HitListFree(hit_list);
    }

BOOST_AUTO_TEST_SUITE_END()
