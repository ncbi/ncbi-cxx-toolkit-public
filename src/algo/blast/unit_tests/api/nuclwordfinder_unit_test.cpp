/* $Id$
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
*   Tests of word finder stage for a nucleotide BLAST search.
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <corelib/ncbitime.hpp>
#include <corelib/ncbistre.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>

#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/na_ungapped.h>

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_exception.hpp>

#include <blast_objmgr_priv.hpp>

#include "test_objmgr.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

struct NuclWordFinderTextFixture
{
    static const int kContigWordSize = 19;
    static const int kDiscontigWindow = 40;
    static const int kDiscontigWordSize = 11;

    BLAST_SequenceBlk* m_pQuery;
    BLAST_SequenceBlk* m_pSubject;
    BlastQueryInfo* m_pQueryInfo;
    BlastOffsetPair* m_Offsets;
    Int4  m_NumHits;
    LookupTableWrap* m_pLookup;
    BlastInitialWordParameters* m_pWordParams;
    Blast_ExtendWord* m_pExtendWord;
    BlastScoreBlk* m_pScoreBlk;
    BlastInitHitList* m_pHitList;
    Int4  m_Range;

    /// Do not create the lookup table, only fill the needed information
    void setupLookupTable(ELookupTableType lut_type, 
                          Int4 word_size, bool disc_mb)
    {
        m_pLookup = (LookupTableWrap*) calloc(1, sizeof(LookupTableWrap));
        m_pLookup->lut_type = lut_type;
        if (lut_type == eMBLookupTable) {
            BlastMBLookupTable* mb_lt = 
                (BlastMBLookupTable*) calloc(1, sizeof(BlastMBLookupTable));
            m_pLookup->lut = (void*) mb_lt;

            mb_lt->word_length = 12;
            mb_lt->lut_word_length = 12;
            if (disc_mb) {
                mb_lt->discontiguous = TRUE;
                mb_lt->scan_step = 4;
                mb_lt->template_length = 21;
            } else {
                mb_lt->scan_step = word_size - 11;
            }
        } else {
            BlastNaLookupTable* blast_lt = 
                (BlastNaLookupTable*) calloc(1, sizeof(BlastNaLookupTable));
            m_pLookup->lut = (void*) blast_lt;
            blast_lt->word_length = word_size;
            blast_lt->lut_word_length = 8;
            blast_lt->scan_step = word_size - 7;
        }
    }
    
    /// Populates matrix in score block, leaving the rest of it unfilled
    void setupScoreBlk()
    {
        const EBlastProgramType kProgram = eBlastTypeBlastn;

        BlastScoringOptions* scoring_options = NULL;
        BlastScoringOptionsNew(kProgram, &scoring_options);
        m_pScoreBlk = BlastScoreBlkNew(BLASTNA_SEQ_CODE, NUM_STRANDS);
        Blast_ScoreBlkMatrixInit(kProgram, scoring_options, m_pScoreBlk, NULL);
        BlastScoringOptionsFree(scoring_options);
    }

    /// Fills sequence buffers
    void setupSequences()
    {
        CSeq_id qid("gi|3090");
        auto_ptr<SSeqLoc> qsl(
            CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_plus));
        SBlastSequence sequence(
             GetSequence(*qsl->seqloc, eBlastEncodingNucleotide, qsl->scope, 
                         eNa_strand_plus, eSentinels));
        Uint1* buf = (Uint1*) calloc((sequence.length+1), sizeof(Uint1));
        memcpy(buf, sequence.data.get(), sequence.length);
        if (BlastSeqBlkNew(&m_pQuery) < 0) {
            NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                       "Query sequence block");
        }

        BlastSeqBlkSetSequence(m_pQuery, buf, sequence.length - 2);
       
        CSeq_id sid("gi|33383640");
        auto_ptr<SSeqLoc> ssl(
            CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_plus));
        SBlastSequence compressed_sequence(
              GetSequence(*ssl->seqloc, eBlastEncodingNcbi2na, ssl->scope,
                          eNa_strand_plus, eNoSentinels));
        if (BlastSeqBlkNew(&m_pSubject) < 0) {
            NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                       "Subject sequence block");
        }
        BlastSeqBlkSetCompressedSequence(m_pSubject,
                                         compressed_sequence.data.release());
        m_pSubject->length = sequence::GetLength(*ssl->seqloc, ssl->scope);

        m_pQueryInfo = BlastQueryInfoNew(eBlastTypeBlastn, 1);
        m_pQueryInfo->contexts[0].query_length = m_pQuery->length;
        m_pQueryInfo->contexts[1].query_offset = m_pQuery->length;
        m_pQueryInfo->contexts[1].is_valid = FALSE;
        m_Range = m_pSubject->length;
    }

    void setupSequenceOffsets(bool disc_mb, bool aligned)
    {
        const int kOffsetArraySize = 1024;
        string datafile = "data/offsets.";

        if (m_pLookup->lut_type == eMBLookupTable)
            datafile += "12";
        else
            datafile += "8";

        if (disc_mb)
            datafile += ".d.txt";
        else if (!aligned)
            datafile += ".rl.txt";
        else
            datafile += ".r.txt"; 

        m_Offsets = 
            (BlastOffsetPair*) calloc(kOffsetArraySize, sizeof(BlastOffsetPair));

        auto_ptr<CNcbiIstream> in(new ifstream(datafile.c_str()));
        string line;
        m_NumHits = 0;

        while ( !in->eof() ) {
            NcbiGetlineEOL(*in, line);
            if (line.empty())
                continue;
            sscanf(line.c_str(), "%u %u", 
                   (Uint4*)&m_Offsets[m_NumHits].qs_offsets.q_off, 
                   (Uint4*)&m_Offsets[m_NumHits].qs_offsets.s_off);
            ++m_NumHits;
        }
        
    }

    void setupExtendWord(ESeedContainerType type, 
                         Int4 window, bool ungapped_ext)
    {
        const int kXDrop = 11;
        const int kCutoff = 14;
        const int kReducedCutoff = 8;
        BlastInitialWordOptions* word_opts = NULL;
        BlastInitialWordOptionsNew(eBlastTypeBlastn, &word_opts);
        word_opts->window_size = window;
        m_pWordParams = (BlastInitialWordParameters*)
            calloc(1, sizeof(BlastInitialWordParameters));
        m_pWordParams->ungapped_extension = (ungapped_ext ? TRUE : FALSE);
        m_pWordParams->options = word_opts;
        m_pWordParams->cutoffs = (BlastUngappedCutoffs *)calloc(1,
                                                sizeof(BlastUngappedCutoffs));
        m_pWordParams->x_dropoff_max = kXDrop;
        m_pWordParams->cutoff_score_min = kCutoff;
        m_pWordParams->cutoffs[0].x_dropoff = kXDrop;
        m_pWordParams->cutoffs[0].cutoff_score = kCutoff;
        m_pWordParams->cutoffs[0].reduced_nucl_cutoff_score = kReducedCutoff;
        m_pWordParams->container_type = type;

        int reward = m_pScoreBlk->reward;
        int penalty = m_pScoreBlk->penalty;
        int *table = m_pWordParams->nucl_score_table;
        for (int i = 0; i < 256; i++) {
            int score = 0;
            if (i & 3) score += penalty; else score += reward;
            if ((i >> 2) & 3) score += penalty; else score += reward;
            if ((i >> 4) & 3) score += penalty; else score += reward;
            if (i >> 6) score += penalty; else score += reward;
            table[i] = score;
        }

        BlastExtendWordNew(m_pQuery->length, m_pWordParams, &m_pExtendWord);
    }

    TNaExtendFunction setupAll(ELookupTableType lut_type, int word_size, 
                       bool disc_mb, ESeedContainerType type, 
                       bool words_aligned, int window, bool ungapped_ext)
    {
        setupLookupTable(lut_type, word_size, disc_mb);
        setupSequences();
        setupScoreBlk();
        setupExtendWord(type, window, ungapped_ext);
        setupSequenceOffsets(disc_mb, words_aligned);
        m_pHitList = BLAST_InitHitListNew();
        BlastChooseNaExtend(m_pLookup);

        if (m_pLookup->lut_type == eMBLookupTable) {
            BlastMBLookupTable *lut = (BlastMBLookupTable *) m_pLookup->lut;
            return (TNaExtendFunction)lut->extend_callback;
        }
        else if (m_pLookup->lut_type == eNaLookupTable) {
            BlastNaLookupTable *lut = (BlastNaLookupTable *) m_pLookup->lut;
            return (TNaExtendFunction)lut->extend_callback;
        }
        else {
            BlastSmallNaLookupTable *lut = 
                                (BlastSmallNaLookupTable *) m_pLookup->lut;
            return (TNaExtendFunction)lut->extend_callback;
        }
    }

    void checkResults()
    {
        const int kNumGoodHits = 5;
        const int kQueryStarts[kNumGoodHits] = { 233, 1037, 263, 1911, 1782 };
        const int kSubjectStarts[kNumGoodHits] = { 0, 66, 27, 811, 940 };
        const int kLengths[kNumGoodHits] = { 31, 945, 685, 101, 63 };
        const int kScores[kNumGoodHits] = { 31, 853, 541, 73, 51 };


        BOOST_REQUIRE_EQUAL(kNumGoodHits, m_pHitList->total);
        for (int index = 0; index < kNumGoodHits; ++index) {
            BOOST_REQUIRE_EQUAL(kQueryStarts[index], 
                m_pHitList->init_hsp_array[index].ungapped_data->q_start);
            BOOST_REQUIRE_EQUAL(kSubjectStarts[index], 
                m_pHitList->init_hsp_array[index].ungapped_data->s_start);
            BOOST_REQUIRE_EQUAL(kLengths[index], 
                m_pHitList->init_hsp_array[index].ungapped_data->length);
            BOOST_REQUIRE_EQUAL(kScores[index], 
                m_pHitList->init_hsp_array[index].ungapped_data->score);
        }
    }

    void checkResultsDisc()
    {
        const int kNumGoodHits = 4;
        const int kQueryStarts[kNumGoodHits] = { 1037, 263, 1911, 1782 };
        const int kSubjectStarts[kNumGoodHits] = { 66, 27, 811, 940 };
        const int kLengths[kNumGoodHits] = { 945, 685, 101, 63 };
        const int kScores[kNumGoodHits] = { 853, 541, 73, 51 };


        BOOST_REQUIRE_EQUAL(kNumGoodHits, m_pHitList->total);
        for (int index = 0; index < kNumGoodHits; ++index) {
            BOOST_REQUIRE_EQUAL(kQueryStarts[index], 
                m_pHitList->init_hsp_array[index].ungapped_data->q_start);
            BOOST_REQUIRE_EQUAL(kSubjectStarts[index], 
                m_pHitList->init_hsp_array[index].ungapped_data->s_start);
            BOOST_REQUIRE_EQUAL(kLengths[index], 
                m_pHitList->init_hsp_array[index].ungapped_data->length);
            BOOST_REQUIRE_EQUAL(kScores[index], 
                m_pHitList->init_hsp_array[index].ungapped_data->score);
        }
    }

    ~NuclWordFinderTextFixture()
    {
        m_pQuery = BlastSequenceBlkFree(m_pQuery);
        m_pSubject = BlastSequenceBlkFree(m_pSubject);
        m_pQueryInfo = BlastQueryInfoFree(m_pQueryInfo);
        sfree(m_Offsets);
        m_pLookup = LookupTableWrapFree(m_pLookup);
        if (m_pWordParams) {
            m_pWordParams->options = 
                BlastInitialWordOptionsFree(m_pWordParams->options);
            m_pWordParams = BlastInitialWordParametersFree(m_pWordParams);
        }
        m_pExtendWord = BlastExtendWordFree(m_pExtendWord);
        m_pScoreBlk = BlastScoreBlkFree(m_pScoreBlk);
        m_pHitList = BLAST_InitHitListFree(m_pHitList);
    }
};

BOOST_FIXTURE_TEST_SUITE(nuclwordfinder, NuclWordFinderTextFixture)

BOOST_AUTO_TEST_CASE(testWordFinder_8_Aligned_Diag)
{
    TNaExtendFunction extend = setupAll(eNaLookupTable, 
                                        kContigWordSize, false, 
                                        eDiagArray, true, 0, true);

    extend(m_Offsets, m_NumHits, m_pWordParams, m_pLookup, 
           m_pQuery, m_pSubject, m_pScoreBlk->matrix->data, 
           m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);
    checkResults();
}

BOOST_AUTO_TEST_CASE(testWordFinder_8_Aligned_Stacks)
{
    TNaExtendFunction extend = setupAll(eNaLookupTable, kContigWordSize, 
                                        false, eDiagHash, true, 0, true);

    extend(m_Offsets, m_NumHits, m_pWordParams, m_pLookup, 
           m_pQuery, m_pSubject, m_pScoreBlk->matrix->data, 
           m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);
    checkResults();
}

BOOST_AUTO_TEST_CASE(testWordFinder_8_Diag)
{
    TNaExtendFunction extend = setupAll(eNaLookupTable, kContigWordSize, 
                                        false, eDiagArray, false, 0, true);

    extend(m_Offsets, m_NumHits,
           m_pWordParams, m_pLookup, m_pQuery, 
           m_pSubject, m_pScoreBlk->matrix->data, 
           m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);
    checkResults();
}

BOOST_AUTO_TEST_CASE(testWordFinder_8_Stacks)
{
    TNaExtendFunction extend = setupAll(eNaLookupTable, kContigWordSize, 
                                        false, eDiagHash, false, 0, true);

    extend(m_Offsets, m_NumHits,
           m_pWordParams, m_pLookup, m_pQuery, 
           m_pSubject, m_pScoreBlk->matrix->data, 
           m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);
    checkResults();
}

BOOST_AUTO_TEST_CASE(testWordFinder_12_Diag_NoUngap)
{
    const int kNumGoodHits = 88;
    TNaExtendFunction extend = setupAll(eMBLookupTable, kContigWordSize, 
                                        false, eDiagArray, false, 0, false);

    extend(m_Offsets, m_NumHits,
           m_pWordParams, m_pLookup, m_pQuery, 
           m_pSubject, m_pScoreBlk->matrix->data, 
           m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);
    BOOST_REQUIRE_EQUAL(kNumGoodHits, m_pHitList->total); 
}

BOOST_AUTO_TEST_CASE(testWordFinder_12_Stacks_NoUngap)
{
    const int kNumGoodHits = 88;
    TNaExtendFunction extend = setupAll(eMBLookupTable, kContigWordSize, 
                                        false, eDiagHash, false, 0, false);

    extend(m_Offsets, m_NumHits,
           m_pWordParams, m_pLookup, m_pQuery, 
           m_pSubject, m_pScoreBlk->matrix->data, 
           m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);
    BOOST_REQUIRE_EQUAL(kNumGoodHits, m_pHitList->total); 
}

BOOST_AUTO_TEST_CASE(testWordFinder_12_Diag)
{
    TNaExtendFunction extend = setupAll(eMBLookupTable, kContigWordSize, 
                                        false, eDiagArray, false, 0, true);

    extend(m_Offsets, m_NumHits,
           m_pWordParams, m_pLookup, m_pQuery, 
           m_pSubject, m_pScoreBlk->matrix->data, 
           m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);
    checkResults();
}

BOOST_AUTO_TEST_CASE(testWordFinder_12_Stacks)
{
    TNaExtendFunction extend = setupAll(eMBLookupTable, kContigWordSize, 
                                        false, eDiagHash, false, 0, true);

    extend(m_Offsets, m_NumHits,
           m_pWordParams, m_pLookup, m_pQuery, 
           m_pSubject, m_pScoreBlk->matrix->data, 
           m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);
    checkResults();
}

BOOST_AUTO_TEST_CASE(testWordFinder_Discontig_Direct_Diag_NoUngap)
{
    const int kNumGoodHits = 31;
    TNaExtendFunction extend = setupAll(eMBLookupTable, kDiscontigWordSize,
                                        true, eDiagArray, false, 
                                        kDiscontigWindow, false);
    int hits_extended = extend(m_Offsets, m_NumHits,
                   m_pWordParams, m_pLookup, m_pQuery, 
                   m_pSubject, m_pScoreBlk->matrix->data, 
                   m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);
    BOOST_REQUIRE_EQUAL(kNumGoodHits, m_pHitList->total); 
    BOOST_REQUIRE_EQUAL(kNumGoodHits, hits_extended); 
}

BOOST_AUTO_TEST_CASE(testWordFinder_Discontig_Direct_Stacks_NoUngap)
{
    const int kNumGoodHits = 31;
    TNaExtendFunction extend = setupAll(eMBLookupTable, kDiscontigWordSize,
                                        true, eDiagHash, false, 
                                        kDiscontigWindow, false);
    int hits_extended = extend(m_Offsets, m_NumHits,
                   m_pWordParams, m_pLookup, m_pQuery, 
                   m_pSubject, m_pScoreBlk->matrix->data, 
                   m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);

    BOOST_REQUIRE_EQUAL(kNumGoodHits, m_pHitList->total); 
    BOOST_REQUIRE_EQUAL(kNumGoodHits, hits_extended); 
}

BOOST_AUTO_TEST_CASE(testWordFinder_Discontig_Direct_Diag)
{
    TNaExtendFunction extend = setupAll(eMBLookupTable, kDiscontigWordSize,
                                        true, eDiagArray, false, 
                                        kDiscontigWindow, true);
    int hits_extended = extend(m_Offsets, m_NumHits,
                   m_pWordParams, m_pLookup, m_pQuery, 
                   m_pSubject, m_pScoreBlk->matrix->data, 
                   m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);
    BOOST_REQUIRE_EQUAL(m_pHitList->total, hits_extended); 
    checkResultsDisc();
}

BOOST_AUTO_TEST_CASE(testWordFinder_Discontig_Direct_Stacks)
{
    TNaExtendFunction extend = setupAll(eMBLookupTable, kDiscontigWordSize,
                                        true, eDiagHash, false, 
                                        kDiscontigWindow, true);
    int hits_extended = extend(m_Offsets, m_NumHits,
                          m_pWordParams, m_pLookup, m_pQuery, 
                          m_pSubject, m_pScoreBlk->matrix->data, 
                          m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);

    BOOST_REQUIRE_EQUAL(m_pHitList->total, hits_extended); 
    checkResultsDisc();
}

/// tests for a bug fix to BlastNaExtend.  For a final 8-mer at the 
/// end of the subject an error was causing it to be extended past the end of 
/// the subject.
BOOST_AUTO_TEST_CASE(testHitAtEndOfSubject)
{
    m_Offsets = NULL; // Not otherwise initialized in this test and then 
                      // incorrectly freed in tearDown.

    CSeq_id qid("gi|516843");
    auto_ptr<SSeqLoc> qsl(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    SBlastSequence sequence(
         GetSequence(*qsl->seqloc, eBlastEncodingNucleotide, qsl->scope, 
                     eNa_strand_both, eSentinels));
    Uint1* buf = (Uint1*) calloc((sequence.length+1), sizeof(Uint1));
    memcpy(buf, sequence.data.get(), sequence.length);
    if (BlastSeqBlkNew(&m_pQuery) < 0) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                   "Query sequence block");
    }

    BlastSeqBlkSetSequence(m_pQuery, buf, sequence.length - 2);
   
    CSeq_id sid("gi|6569");
    auto_ptr<SSeqLoc> ssl(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_plus));
    SBlastSequence compressed_sequence(
          GetSequence(*ssl->seqloc, eBlastEncodingNcbi2na, ssl->scope,
                      eNa_strand_plus, eNoSentinels));
    if (BlastSeqBlkNew(&m_pSubject) < 0) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                   "Subject sequence block");
    }
    BlastSeqBlkSetCompressedSequence(m_pSubject,
                                     compressed_sequence.data.release());
    m_pSubject->length = sequence::GetLength(*ssl->seqloc, ssl->scope);

    m_pQueryInfo = BlastQueryInfoNew(eBlastTypeBlastn, 1);
    m_pQueryInfo->contexts[0].query_length = m_pQuery->length;
    m_pQueryInfo->contexts[1].query_offset = m_pQuery->length;
    m_pQueryInfo->contexts[1].is_valid = FALSE;

    m_Range = m_pSubject->length;

    const int kOffsetArraySize = 4;
    Uint4 QOffset[kOffsetArraySize] = {1254, 502, 896, 1170};
    Uint4 SOffset[kOffsetArraySize] = {12, 84, 320, 420};
    BlastOffsetPair offset_pairs[kOffsetArraySize];
    for (int index = 0; index < kOffsetArraySize; ++index) {
        offset_pairs[index].qs_offsets.q_off = QOffset[index];
        offset_pairs[index].qs_offsets.s_off = SOffset[index];
    }

    setupLookupTable(eNaLookupTable, 11, FALSE);
    setupScoreBlk();
    setupExtendWord(eDiagArray, 0, FALSE);
    m_pHitList = BLAST_InitHitListNew();

    BlastChooseNaExtend(m_pLookup);
    TNaExtendFunction extend = (TNaExtendFunction) 
             (((BlastNaLookupTable *)(m_pLookup->lut))->extend_callback);

    int hits_extended = extend(offset_pairs, kOffsetArraySize, 
        m_pWordParams, m_pLookup, m_pQuery, m_pSubject, 
        m_pScoreBlk->matrix->data, m_pQueryInfo, m_pExtendWord, m_pHitList, m_Range);

    BOOST_REQUIRE_EQUAL(0, hits_extended);
}

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: nuclwordfinder-cppunit.cpp,v $
* Revision 1.24  2009/10/02 15:46:12  maning
* Conform to the changes related to JIRA SB-411.
*
* Revision 1.23  2009/04/22 12:53:05  maning
* Change of signature in extend() to support soft subject masks.
*
* Revision 1.22  2008/12/22 14:51:00  maning
* The original ungapped extension algorithm used min_step (scan_step) to determine the minimal overlap of current hit with the previous saved one.  The new algorithm uses word_length instead, and therefore causes hit_extended to change in cases where no ungapped extension is performed.  This updates the extended hit counts.
*
* Revision 1.21  2007/10/23 16:00:57  madden
* Changes for removal of [SG]etUngappedExtension
*
* Revision 1.20  2007/01/05 16:20:26  papadopo
* change the interface to the ungapped extension routines
*
* Revision 1.19  2006/12/13 19:22:53  papadopo
* change test names to reflect renamed extension routines; also remove use of variable seed extension method
*
* Revision 1.18  2006/11/21 17:47:10  papadopo
* rearrange headers, change lookup table type, use enums for lookup table constants
*
* Revision 1.17  2006/09/01 15:11:36  papadopo
* change initialization to reflect per-context cutoffs
*
* Revision 1.16  2006/07/31 18:29:17  coulouri
* refactor access to diagonal hash, use standard nomenclature
*
* Revision 1.15  2006/07/27 16:24:40  coulouri
* remove blast_extend_priv.h
*
* Revision 1.14  2006/06/05 13:34:05  madden
* Changes to remove [GS]etMatrixPath and use callback instead
*
* Revision 1.13  2006/01/03 18:00:19  papadopo
* 1. Manually initialize fields needed for approximate ungapped
*    extension
* 2. Switch order of initializing structures, since InitialWordParameters
*    depends on a filled-in score block
*
* Revision 1.12  2005/12/19 16:39:33  papadopo
* 1. Remove tests for now-deleted extension method
* 2. Remove use of now-deleted structure fields
*
* Revision 1.11  2005/07/07 16:32:45  camacho
* Revamping of BLAST exception classes and error codes
*
* Revision 1.10  2005/06/23 19:07:04  madden
* Adjustment for fix to 2-hit dcmb
*
* Revision 1.9  2005/06/09 20:37:06  camacho
* Use new private header blast_objmgr_priv.hpp
*
* Revision 1.8  2005/05/16 19:03:47  papadopo
* 1. Remove tests for nucleotide ungapped extensions that the engine
*    can no longer perform
* 2. Use DiscMB_ExtendInitialHits instead of MB_ExtendInitialHits
*
* Revision 1.7  2005/05/10 16:09:04  camacho
* Changed *_ENCODING #defines to EBlastEncoding enumeration
*
* Revision 1.6  2005/03/28 21:24:08  dondosha
* Use an offset pair union of structures instead of two offset arrays in ScanSubject and WordFinder routines, because of potentially different meanings of returned offsets
*
* Revision 1.5  2005/03/04 17:20:45  bealer
* - Command line option support.
*
* Revision 1.4  2005/02/28 17:14:55  dondosha
* Changes due to signature changes in nucleotide initial seed extension functions
*
* Revision 1.3  2005/02/14 14:30:36  camacho
* added missing headers
*
* Revision 1.2  2005/02/14 14:17:17  camacho
* Changes to use SBlastScoreMatrix
*
* Revision 1.1  2005/02/11 15:09:15  camacho
* Renaming of wordfinder-cppunit.cpp -> nuclwordfinder-cppunit.cpp
*
* Revision 1.8  2005/01/25 17:28:57  coulouri
* fix alignment problem which resulted in a bus error on sparc64
*
* Revision 1.7  2005/01/10 14:05:46  madden
* Moved extension method and container type from InitialWordOptions to parameters
*
* Revision 1.6  2005/01/05 18:21:26  madden
* Add testHitAtEndOfSubject to test bug fix in BlastNaExtendRight
*
* Revision 1.5  2004/12/28 16:48:26  camacho
* 1. Use typedefs to AutoPtr consistently
* 2. Use SBlastSequence structure instead of std::pair as return value to
*    blast::GetSequence
*
* Revision 1.4  2004/11/24 15:27:22  camacho
* Renamed private headers from *_pri.h to *_priv.h
*
* Revision 1.3  2004/08/30 16:53:53  dondosha
* Added E in front of enum names ESeedExtensionMethod and ESeedContainerType
*
* Revision 1.2  2004/08/05 20:42:52  dondosha
* Check stacks container with various extension methods; check hits in more details; check cases when ungapped extension is not done
*
* ===========================================================================
*/
