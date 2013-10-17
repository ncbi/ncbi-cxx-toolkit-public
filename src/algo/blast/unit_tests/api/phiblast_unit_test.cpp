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
* Author:  Ilya Dondoshansky
*
* File Description:
*   Unit test module to test the PHI BLAST functions.
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/phi_lookup.h>
#include <algo/blast/core/phi_gapalign.h>
#include <blast_setup.hpp>

using namespace std;
USING_NCBI_SCOPE;
USING_SCOPE(blast);

class CPhiblastTestFixture {

public:
    CBlastScoreBlk m_ScoreBlk;
    CLookupTableWrap m_Lookup;
    CBlastQueryInfo m_QueryInfo;
    EBlastProgramType m_Program;

    void x_SetupSequenceBlk(const string& seq, BLAST_SequenceBlk** seq_blk) {
        BlastSeqBlkNew(seq_blk);
        Uint1* buffer = (Uint1*) malloc(seq.size() + 2);
        buffer[0] = buffer[seq.size()+1] = 0;
        memcpy(buffer+1, seq.c_str(), seq.size());
        // Convert to ncbistdaa encoding
        for (unsigned int index = 1; index <= seq.size(); ++index)
            buffer[index] = AMINOACID_TO_NCBISTDAA[buffer[index]];

        BlastSeqBlkSetSequence(*seq_blk, buffer, seq.size());
    }

    /// After the initial set-up is done, finds pattern occurrences in query and
    /// fills the pattern information in the BlastQueryInfo structure.
    void x_FindQueryOccurrences(void) {
        const string kQuerySeq("GPLRQIFVEFLERSCTAEFSGFLLYKELGRRLKKTNPVVAEIFSLMSR"
                               "DEARHAGFLNKGLSDFNLALDLGFLTKARKYTFFKPKFIFYATYLSEK"
                               "IGYWRYITIFRHLKANPEYQVYPIFKYFENWCQDENRHGDFFSALL");
        SPHIPatternSearchBlk* pattern_blk = (SPHIPatternSearchBlk*) m_Lookup->lut;
        CBLAST_SequenceBlk query_blk;
        x_SetupSequenceBlk(kQuerySeq, &query_blk);
        BlastSeqLoc* location = NULL;
        CBlast_Message blast_msg;
        BlastSeqLocNew(&location, 0, kQuerySeq.size()-1);
        Blast_SetPHIPatternInfo(m_Program, pattern_blk, query_blk, 
                                location, m_QueryInfo, &blast_msg);
        location = BlastSeqLocFree(location);
    }

    void x_CheckGappedAlignmentResults(BlastGapAlignStruct* gap_align) {
        BOOST_REQUIRE_EQUAL(8, gap_align->score);
        BOOST_REQUIRE_EQUAL(94, gap_align->query_start);
        BOOST_REQUIRE_EQUAL(142, gap_align->query_stop);
        BOOST_REQUIRE_EQUAL(8, gap_align->subject_start);
        BOOST_REQUIRE_EQUAL(61, gap_align->subject_stop);
        /* Check several values in the edit script. */
        BOOST_REQUIRE_EQUAL(3, gap_align->edit_script->num[0]);
        BOOST_REQUIRE_EQUAL(5, gap_align->edit_script->num[1]);
        GapEditScript* esp = gap_align->edit_script;
        BOOST_REQUIRE_EQUAL(3, esp->size);
        BOOST_REQUIRE_EQUAL(45, esp->num[2]);
    }

    /// Set up: initializes the PHI "lookup table", aka the SPHIPatternSearchBlk
    /// structure, the score block and the query information structure.
    CPhiblastTestFixture() {
        m_Program = eBlastTypePhiBlastp;
        CBlastScoringOptions score_options;
        BlastScoringOptionsNew(m_Program, &score_options);
        CBlast_Message msg;
        // Nothing is needed from BlastQueryInfo except that it's allocated,
        // and last_context is set to 0.
        m_QueryInfo.Reset(BlastQueryInfoNew(m_Program, 1));
        // In PHI BLAST, query block is not needed neither for score block setup,
        // nor for lookup table set up!
        BlastSetup_ScoreBlkInit(NULL, m_QueryInfo, score_options, m_Program, 
                                &m_ScoreBlk, 1.0, &msg, &BlastFindMatrixPath);

    }

    void setUpLookupTable(string pattern)
    {
        CLookupTableOptions lookup_options;
        LookupTableOptionsNew(m_Program, &lookup_options);
        lookup_options->phi_pattern = strdup(pattern.c_str());
        // Lookup segments and rps info arguments are irrelevant and passed as 
        // NULL.
        LookupTableWrapInit(NULL, lookup_options, NULL, NULL, m_ScoreBlk, &m_Lookup, NULL, NULL);
    }

    ~CPhiblastTestFixture() {
         m_ScoreBlk.Reset();
         m_Lookup.Reset();
    }

    static SPHIQueryInfo* x_SetupPatternInfo(void) {
        const int kNumPatterns = 4;
        const SPHIPatternInfo kPatOccurrences[kNumPatterns] = 
            { {100,20}, {200,18}, {300,22}, {400, 21} };
        SPHIQueryInfo* pat_info = SPHIQueryInfoNew(); 
        pat_info->num_patterns = pat_info->allocated_size = kNumPatterns;
        // Occurrences array has already been allocated to size 8, so memcpy
        // is safe here.
        memcpy(pat_info->occurrences, kPatOccurrences, 
               kNumPatterns*sizeof(SPHIPatternInfo));
    
        return pat_info;
    }
    
    static BlastHSPList* x_SetupHSPList(int index) {
        const int kHspMax = 10;
        const int kNumRepetitions = 4;
        BlastHSPList* hsp_list = Blast_HSPListNew(kHspMax);
        hsp_list->oid = index;
        for (int hsp_index = 0; hsp_index < kHspMax; ++hsp_index) {
            BlastHSP* hsp = Blast_HSPNew();
            hsp->score = 200 - 2*index - 5*hsp_index;
            hsp->evalue = ((double)1)/hsp->score;
            hsp->pat_info = (SPHIHspInfo*) calloc(1, sizeof(SPHIHspInfo));
            hsp->pat_info->index = hsp_index % kNumRepetitions;
            Blast_HSPListSaveHSP(hsp_list, hsp);
        }
        return hsp_list;
    }
    
    static BlastHSPResults* x_SetupResults(const int kHitlistSize) {
        BlastHSPResults* results = Blast_HSPResultsNew(1);
        for (int index = 0; index < kHitlistSize; ++index) {
            BlastHSPList* hsp_list = x_SetupHSPList(index);
            Blast_HSPResultsInsertHSPList(results, hsp_list, kHitlistSize);
        }
        return results;
    }
    
    static bool 
    x_CheckIncreasingBestEvalues(BlastHitList* hitlist) {
        int index;
        for (index = 0; index < hitlist->hsplist_count - 1; ++index) {
            if (hitlist->hsplist_array[index]->best_evalue > 
                hitlist->hsplist_array[index+1]->best_evalue)
                break;
        }
        return (index == hitlist->hsplist_count - 1);
    }
    
    static void 
    x_CheckSplitResults(BlastHSPResults** results_array, int num_results)
    {
        const int kNumHspLists = 20;
        for (int hitlist_index = 0; hitlist_index < num_results; 
             ++hitlist_index) {
            BOOST_REQUIRE(results_array[hitlist_index] != NULL);
            BlastHitList* hitlist = 
                results_array[hitlist_index]->hitlist_array[0];
            BOOST_REQUIRE_EQUAL(kNumHspLists, hitlist->hsplist_count);
            BOOST_REQUIRE(x_CheckIncreasingBestEvalues(hitlist));
            const int kHspCnt = (13-hitlist_index)/num_results;
            for (int hsplist_index = 0; hsplist_index < kNumHspLists; 
                 ++hsplist_index) {
                BlastHSPList* hsplist = hitlist->hsplist_array[hsplist_index];
                BOOST_REQUIRE_EQUAL(kHspCnt, hsplist->hspcnt);
                BOOST_REQUIRE_EQUAL(hsplist_index, hsplist->oid);
                BOOST_REQUIRE(Blast_HSPListIsSortedByScore(hsplist) == TRUE);
                for (int hsp_index = 0; hsp_index < kHspCnt; ++hsp_index) {
                    BlastHSP* hsp = hsplist->hsp_array[hsp_index];
                    BOOST_REQUIRE_EQUAL(hitlist_index, 
                                         hsp->pat_info->index);
                }
            }
            results_array[hitlist_index] = 
                Blast_HSPResultsFree(results_array[hitlist_index]);
        }
        sfree(results_array);
    }
    
};

BOOST_FIXTURE_TEST_SUITE(phiblast, CPhiblastTestFixture)

/// Tests the values in the PHI BLAST lookup table.
BOOST_AUTO_TEST_CASE(testPHILookupTableLong) {
    setUpLookupTable("[ED]-x(32,40)-E-x(2)-H");
    // Test score block contents
    BOOST_REQUIRE(m_ScoreBlk->kbp_gap == m_ScoreBlk->kbp_gap_std);
    BOOST_REQUIRE(m_ScoreBlk->kbp == m_ScoreBlk->kbp_std);
    BOOST_REQUIRE_EQUAL(0.5, m_ScoreBlk->kbp_gap[0]->paramC);
    BOOST_REQUIRE(m_ScoreBlk->kbp_gap[0]->H != 0);
    BOOST_REQUIRE_EQUAL(m_ScoreBlk->kbp[0]->Lambda, 
                         m_ScoreBlk->kbp_gap[0]->Lambda);
    BOOST_REQUIRE_EQUAL(m_ScoreBlk->kbp[0]->K, m_ScoreBlk->kbp_gap[0]->K);

    // Test pattern items structure contents
    SPHIPatternSearchBlk* pattern_blk = (SPHIPatternSearchBlk*) m_Lookup->lut;

    BOOST_REQUIRE(pattern_blk->flagPatternLength == eVeryLong);
    BOOST_REQUIRE_EQUAL(37, pattern_blk->minPatternMatchLength);
    BOOST_REQUIRE_CLOSE(0.0013, pattern_blk->patternProbability, 1);
    BOOST_REQUIRE_EQUAL(3, pattern_blk->multi_word_items->numWords);
    BOOST_REQUIRE(pattern_blk->multi_word_items->extra_long_items != NULL);
}

/// Tests the values in the PHI BLAST lookup table.
BOOST_AUTO_TEST_CASE(testPHILookupTableShort) {
    setUpLookupTable("LLY");
    // Test score block contents
    BOOST_REQUIRE(m_ScoreBlk->kbp_gap == m_ScoreBlk->kbp_gap_std);
    BOOST_REQUIRE(m_ScoreBlk->kbp == m_ScoreBlk->kbp_std);
    BOOST_REQUIRE_EQUAL(0.5, m_ScoreBlk->kbp_gap[0]->paramC);
    BOOST_REQUIRE(m_ScoreBlk->kbp_gap[0]->H != 0);
    BOOST_REQUIRE_EQUAL(m_ScoreBlk->kbp[0]->Lambda, 
                         m_ScoreBlk->kbp_gap[0]->Lambda);
    BOOST_REQUIRE_EQUAL(m_ScoreBlk->kbp[0]->K, m_ScoreBlk->kbp_gap[0]->K);

    // Test pattern items structure contents
    SPHIPatternSearchBlk* pattern_blk = (SPHIPatternSearchBlk*) m_Lookup->lut;

    BOOST_REQUIRE(pattern_blk->flagPatternLength == eOneWord);
    BOOST_REQUIRE_EQUAL(3, pattern_blk->minPatternMatchLength);
    BOOST_REQUIRE_CLOSE(0.000262, pattern_blk->patternProbability, 1);
    BOOST_REQUIRE_EQUAL(0, pattern_blk->multi_word_items->numWords);
    BOOST_REQUIRE(pattern_blk->multi_word_items->extra_long_items == NULL);
    BOOST_REQUIRE_EQUAL(4, pattern_blk->one_word_items->match_mask);
    BOOST_REQUIRE(pattern_blk->one_word_items->whichPositionPtr != NULL);
}

/// Tests the finding of pattern occurrences in query.
BOOST_AUTO_TEST_CASE(testFindQueryOccurrencesLong) {
    setUpLookupTable("[ED]-x(32,40)-E-x(2)-H");
    x_FindQueryOccurrences();
    SPHIQueryInfo* pattern_info = m_QueryInfo->pattern_info;
    BOOST_REQUIRE(pattern_info != NULL);
    BOOST_REQUIRE_EQUAL(3, pattern_info->num_patterns);
    BOOST_REQUIRE_CLOSE(0.0013, pattern_info->probability, 1);
    // Check that minimal pattern length has been saved in the length 
    // adjustment field.
    BOOST_REQUIRE_EQUAL(37, m_QueryInfo->contexts[0].length_adjustment);
}

/// Tests the finding of pattern occurrences in query.
BOOST_AUTO_TEST_CASE(testFindQueryOccurrencesShort) {
    setUpLookupTable("LLY");
    x_FindQueryOccurrences();
    SPHIQueryInfo* pattern_info = m_QueryInfo->pattern_info;
    BOOST_REQUIRE(pattern_info != NULL);
    BOOST_REQUIRE_EQUAL(1, pattern_info->num_patterns);
    BOOST_REQUIRE_CLOSE(0.000262, pattern_info->probability, 1);
    // Check that minimal pattern length has been saved in the length 
    // adjustment field.
    BOOST_REQUIRE_EQUAL(3, m_QueryInfo->contexts[0].length_adjustment);
}

/// Tests PHI BLAST calculation of e-values
BOOST_AUTO_TEST_CASE(testPHICalcEvalues) {
    const int kNumDbHits = 33;
    setUpLookupTable("[ED]-x(32,40)-E-x(2)-H");
    x_FindQueryOccurrences();

    SPHIPatternSearchBlk pattern_blk;
    pattern_blk.num_patterns_db = kNumDbHits;
    
    BlastHSPList* hsp_list = Blast_HSPListNew(0);
    hsp_list->hspcnt = 1;

    BlastHSP* hsp = hsp_list->hsp_array[0] = Blast_HSPNew();
    hsp->score = 527;
    
    Blast_HSPListPHIGetEvalues(hsp_list, m_ScoreBlk, m_QueryInfo, &pattern_blk);

    BOOST_REQUIRE_CLOSE(7.568e-59, hsp->evalue, 1);

    hsp_list = Blast_HSPListFree(hsp_list);
}

/// Tests finding of pattern occurrences in subject.
BOOST_AUTO_TEST_CASE(testPHIScanSubject) {
    setUpLookupTable("[ED]-x(32,40)-E-x(2)-H");
    const string 
        kSubjectSeq("GETRKLFVEFLERSCTAEFSGFLLYKELGRRLKGKSPVLAECFNLMSRDEARHAG"
                    "FLNKALSDFNLSLDLGFLTKSRNYTFFKPKFIFYATYLSEKIGYWRYITIYRHLE"
                    "AHPEDRVYPIFRFFENWCQDENRHGDFFDAIMKSQPQILNDWKARLWSRF");
    const int kNumHits = 3;
    const int kStarts[kNumHits] = { 8,  11,  94 };
    const int kEnds[kNumHits]   = { 52, 52, 133 };
 
    Int4 start_offset = 0;
    CBLAST_SequenceBlk subject_blk;
    x_SetupSequenceBlk(kSubjectSeq, &subject_blk);
    BlastOffsetPair* offset_pairs = (BlastOffsetPair*)
        calloc(GetOffsetArraySize(m_Lookup), sizeof(BlastOffsetPair));
    // Query block and array size arguments are not used when scanning 
    // subject for pattern hits, so pass NULL and 0 for respective arguments.
    Int4 hit_count = 
        PHIBlastScanSubject(m_Lookup, NULL, subject_blk, &start_offset,
                            offset_pairs, 0);
    BOOST_REQUIRE_EQUAL(kNumHits, hit_count);
    for (int index = 0; index < kNumHits; ++index) {
        BOOST_REQUIRE_EQUAL(kStarts[index], 
                             (int) offset_pairs[index].phi_offsets.s_start);
        BOOST_REQUIRE_EQUAL(kEnds[index], 
                             (int) offset_pairs[index].phi_offsets.s_end);
    }
    sfree(offset_pairs);
}

BOOST_AUTO_TEST_CASE(testPHIGappedAlignmentWithTraceback) {
    setUpLookupTable("[ED]-x(32,40)-E-x(2)-H");
    const string 
        kQuerySeq("GPLRQIFVEFLERSCTAEFSGFLLYKELGRRLKKTNPVVAEIFSLMSRDEARHAGFL"
                  "NKGLSDFNLALDLGFLTKARKYTFFKPKFIFYATYLSEKIGYWRYITIFRHLKANPE"
                  "YQVYPIFKYFENWCQDENRHGDFFSALL");
    const string 
        kSubjectSeq("GETRKLFVEFLERSCTAEFSGFLLYKELGRRLKGKSPVLAECFNLMSRDEARHAG"
                    "FLNKALSDFNLSLDLGFLTKSRNYTFFKPKFIFYATYLSEKIGYWRYITIYRHLE"
                    "AHPEDRVYPIFRFFENWCQDENRHGDFFDAIMKSQPQILNDWKARLWSRF");
    const int kQueryPatLength = 40;
    const int kQueryStart = 94;
    CBLAST_SequenceBlk query_blk;
    x_SetupSequenceBlk(kQuerySeq, &query_blk);
    CBLAST_SequenceBlk subject_blk;
    x_SetupSequenceBlk(kSubjectSeq, &subject_blk);
    const int kSubjectPatLength = 45;
    const int kSubjectStart = 8;
    
    CBlastScoringOptions score_opts;
    BlastScoringOptionsNew(m_Program, &score_opts);
    CBlastScoringParameters score_params;
    BlastScoringParametersNew(score_opts, m_ScoreBlk, &score_params);
    CBlastExtensionOptions ext_opts;
    BlastExtensionOptionsNew(m_Program, &ext_opts, score_opts->gapped_calculation);
    CBlastExtensionParameters ext_params;
    BlastExtensionParametersNew(m_Program, ext_opts, m_ScoreBlk, 
                                m_QueryInfo, &ext_params);
    CBlastGapAlignStruct gap_align;

    BLAST_GapAlignStructNew(score_params, ext_params, 
                                subject_blk->length, m_ScoreBlk, 
                                &gap_align);

    SPHIPatternSearchBlk* pattern_blk = 
        (SPHIPatternSearchBlk*) m_Lookup->lut;
    PHIGappedAlignmentWithTraceback(query_blk->sequence, 
                                    subject_blk->sequence, gap_align, 
                                    score_params, kQueryStart, kSubjectStart, 
                                    query_blk->length, subject_blk->length,
                                    kQueryPatLength, kSubjectPatLength, 
                                    pattern_blk);

    x_CheckGappedAlignmentResults(gap_align);
}

BOOST_AUTO_TEST_CASE(testPHIBlastHSPResultsSplit) {
    setUpLookupTable("[ED]-x(32,40)-E-x(2)-H");
    SPHIQueryInfo* pattern_info = x_SetupPatternInfo();
    BlastHSPResults* results = x_SetupResults(20);
    
    BlastHSPResults** results_array = 
        PHIBlast_HSPResultsSplit(results, pattern_info);

    x_CheckSplitResults(results_array, pattern_info->num_patterns);
    results = Blast_HSPResultsFree(results);
    pattern_info = SPHIQueryInfoFree(pattern_info);
}

BOOST_AUTO_TEST_CASE(testPHIBlastHSPResultsSplitNoHits) {
    setUpLookupTable("[ED]-x(32,40)-E-x(2)-H");
    SPHIQueryInfo* pattern_info = x_SetupPatternInfo();
    BlastHSPResults* results = x_SetupResults(0);
    
    BlastHSPResults** results_array = 
        PHIBlast_HSPResultsSplit(results, pattern_info);

    BOOST_REQUIRE(results_array != NULL);
    BOOST_REQUIRE(results_array[0] == NULL);

    sfree(results_array);
    results = Blast_HSPResultsFree(results);
    pattern_info = SPHIQueryInfoFree(pattern_info);
}

// mainly tests cutoff score.  Would more logically belong in blastoptions-cppunit.cpp,
// but set up functions are here.
BOOST_AUTO_TEST_CASE(testPHIBlastHitSavingParameters) {
    const EBlastProgramType kBlastProgram = eBlastTypePhiBlastp;
    const bool kIsGapped = true;
    setUpLookupTable("[ED]-x(32,40)-E-x(2)-H");
    x_FindQueryOccurrences();

    BlastExtensionOptions* ext_options = NULL;
    BlastExtensionOptionsNew(kBlastProgram, &ext_options, kIsGapped);

    BlastHitSavingOptions* hit_options;
    BlastHitSavingOptionsNew(kBlastProgram, &hit_options, kIsGapped);

    m_QueryInfo->contexts[0].eff_searchsp = 10000000;
    const int k_avg_subject_length=343;
    BlastHitSavingParameters* hit_params;
    BlastHitSavingParametersNew(kBlastProgram, hit_options, m_ScoreBlk, m_QueryInfo, k_avg_subject_length, 0, &hit_params);

    BOOST_REQUIRE_EQUAL(28, hit_params->cutoffs[0].cutoff_score);
    BOOST_REQUIRE_EQUAL(28, hit_params->cutoff_score_min);

    ext_options = BlastExtensionOptionsFree(ext_options);
    hit_params = BlastHitSavingParametersFree(hit_params);
    hit_options = BlastHitSavingOptionsFree(hit_options);
}


BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: phiblast-cppunit.cpp,v $
* Revision 1.14  2008/01/31 22:07:00  madden
* Change call to LookupTableWrapInit as part of fix for SB-44
*
* Revision 1.13  2007/10/22 19:16:10  madden
* BlastExtensionOptionsNew has Boolean gapped arg
*
* Revision 1.12  2006/11/16 15:17:28  madden
* Add testPHIBlastHSPResultsSplitNoHits
*
* Revision 1.11  2006/09/15 13:12:05  madden
* Change to LookupTableWrapInit prototype
*
* Revision 1.10  2006/09/01 15:12:10  papadopo
* change name of cutoff values to check
*
* Revision 1.9  2006/07/19 13:30:36  madden
* Refactored setup to allow different patterns.
* Added tests for short pattern.
* Added tearDown method
*
* Revision 1.8  2006/06/29 16:25:24  camacho
* Changed BlastHitSavingOptionsNew signature
*
* Revision 1.7  2006/06/05 13:34:05  madden
* Changes to remove [GS]etMatrixPath and use callback instead
*
* Revision 1.6  2006/05/22 13:34:00  madden
* Add testPHIBlastHitSavingParameters
*
* Revision 1.5  2006/02/15 15:09:43  madden
* Changes for GapEditScript structure change
*
* Revision 1.4  2006/01/12 20:42:51  camacho
* Fix calls to BLAST_MainSetUp to include Blast_Message argument, use BlastQueryInfoNew
*
* Revision 1.3  2005/05/26 14:43:51  dondosha
* Added testPHIBlastHSPResultsSplit to check splitting of PHI BLAST results into an array of results corresponding to different pattern occurrences
*
* Revision 1.2  2005/05/04 16:15:00  papadopo
* modify expected traceback to account for bugfixes in engine
*
* Revision 1.1  2005/04/27 20:09:56  dondosha
* PHI BLAST unit tests
*
*
* ===========================================================================
*/
