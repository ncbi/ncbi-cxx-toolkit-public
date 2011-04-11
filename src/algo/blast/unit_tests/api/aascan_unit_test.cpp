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
* Author: Jason Papadopoulos
*
* File Description:
*   Protein subject scan unit tests
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
#include "blast_objmgr_priv.hpp"

#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_aalookup.h>
#include <algo/blast/core/blast_aascan.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/lookup_util.h>

#include "test_objmgr.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
int compare_offsets(const void *x, const void *y)
{
    BlastOffsetPair *xx = (BlastOffsetPair *)x;
    BlastOffsetPair *yy = (BlastOffsetPair *)y;

    if (xx->qs_offsets.s_off > yy->qs_offsets.s_off)
        return 1;
    if (xx->qs_offsets.s_off < yy->qs_offsets.s_off)
        return -1;

    if (xx->qs_offsets.q_off > yy->qs_offsets.q_off)
        return 1;
    if (xx->qs_offsets.q_off < yy->qs_offsets.q_off)
        return -1;
    return 0;
}

/// The test fixture to use for all test cases in this file
struct AascanTestFixture {

    BLAST_SequenceBlk *query_blk;
    BLAST_SequenceBlk *subject_blk;
    LookupTableWrap* lookup_wrap_ptr;
    LookupTableOptions* lookup_options;
    BlastScoringOptions* score_options;
    BlastScoreBlk *sbp;
    BlastOffsetPair *offset_pairs;

    AascanTestFixture()
    {
        Int4 status;

        //------------------------------------------ QUERY SETUP -----------
        // load the query
        CSeq_id id("gi|18652417");
        
        auto_ptr<SSeqLoc> ssl
            (CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_unknown));

        SBlastSequence query_sequence = 
                    GetSequence(*ssl->seqloc,
                                eBlastEncodingProtein,
                                ssl->scope,
                                eNa_strand_unknown, // strand not applicable
                                eNoSentinels);      // nucl sentinel not applicable

        // create the sequence block

        query_blk = NULL;
        status = BlastSeqBlkNew(&query_blk);
        BOOST_REQUIRE_EQUAL(0, status);
        status = BlastSeqBlkSetSequence(query_blk, query_sequence.data.release(),
                                query_sequence.length - 2);
        BOOST_REQUIRE_EQUAL(0, status);

        BOOST_REQUIRE(query_blk != NULL);
        BOOST_REQUIRE(query_blk->sequence[0] != 0);
        BOOST_REQUIRE(query_blk->sequence[query_blk->length - 1] != 0);
        BOOST_REQUIRE(query_blk->sequence_start[0] == 0);
        BOOST_REQUIRE(query_blk->sequence_start[query_blk->length + 1] == 0);
        BOOST_REQUIRE_EQUAL(0, (int)query_blk->num_seq_ranges);

        // indicate which regions of the query to index

        BlastSeqLoc* lookup_segments = NULL;
        BlastSeqLocNew(&lookup_segments, 0, query_sequence.length - 3);

        //------------------------------------------ SUBJECT SETUP -----------
        // load the subject
        CSeq_id subject_id("gi|7481886");
        
        auto_ptr<SSeqLoc> subject_ssl
            (CTestObjMgr::Instance().CreateSSeqLoc(subject_id,
                                                   eNa_strand_unknown));

        SBlastSequence subj_sequence =
                    GetSequence(*subject_ssl->seqloc,
                                eBlastEncodingProtein,
                                subject_ssl->scope,
                                eNa_strand_unknown, // strand not applicable
                                eNoSentinels);      // nucl sentinel not applicable

        // create the subject sequence block

        subject_blk = NULL;
        status = BlastSeqBlkNew(&subject_blk);
        BOOST_REQUIRE_EQUAL(0, status);
        status = BlastSeqBlkSetSequence(subject_blk, 
                                        subj_sequence.data.release(),
                                        subj_sequence.length - 2);
        BOOST_REQUIRE_EQUAL(0, status);

        SSeqRange full_range;
        full_range.left = 0;
        full_range.right = subject_blk->length;
        status = BlastSeqBlkSetSeqRanges(subject_blk, &full_range, 1, true, eNoSubjMasking);
        BOOST_REQUIRE_EQUAL(0, status);

        BOOST_REQUIRE(subject_blk != NULL);
        BOOST_REQUIRE(subject_blk->sequence[0] != 0);
        BOOST_REQUIRE(subject_blk->sequence[subject_blk->length - 1] != 0);
        BOOST_REQUIRE(subject_blk->sequence_start[0] == 0);
        BOOST_REQUIRE(subject_blk->sequence_start[subject_blk->length + 1] == 0);
        BOOST_REQUIRE_EQUAL(1, (int)subject_blk->num_seq_ranges);

        //----------------------------------- LOOKUP TABLE SETUP -----------
        // set lookup table options

        status = LookupTableOptionsNew(eBlastTypeBlastp, &lookup_options);
        BOOST_REQUIRE_EQUAL(0, status);
        status = BLAST_FillLookupTableOptions(lookup_options,
                                     eBlastTypeBlastp, 
                                     FALSE,  // megablast
                                     BLAST_WORD_THRESHOLD_BLASTP, // threshold
                                     3);     // word size
        BOOST_REQUIRE_EQUAL(0, status);
        
        // get ready to fill in the scoring matrix

        status = BlastScoringOptionsNew(eBlastTypeBlastp, &score_options);
        BOOST_REQUIRE_EQUAL(0, status);
        status = BLAST_FillScoringOptions(score_options, 
                                 eBlastTypeBlastp, 
                                 FALSE,                     // greedy
                                 0,                         // match value
                                 0,                         // mismatch value
                                 NULL,                      // score matrix
                                 BLAST_GAP_OPEN_PROT,       // gap open
                                 BLAST_GAP_EXTN_PROT        // gap extend
                                 );
        BOOST_REQUIRE_EQUAL(0, status);

        // fill in the scoring matrix

        sbp = BlastScoreBlkNew(BLASTAA_SEQ_CODE, 1);
        BOOST_REQUIRE(sbp != NULL);
        status = Blast_ScoreBlkMatrixInit(eBlastTypeBlastp, score_options, sbp,
              &BlastFindMatrixPath);
        BOOST_REQUIRE_EQUAL(0, status);
        
        // create the lookup table

        status = LookupTableWrapInit(query_blk,
                            lookup_options,
                            NULL,
                            lookup_segments,
                            sbp,
                            &lookup_wrap_ptr,
                            NULL /* RPS Info */,
                            NULL);
        BOOST_REQUIRE_EQUAL(0, status);

        lookup_segments = BlastSeqLocFree(lookup_segments);

        // create the hit collection array
        
        offset_pairs = (BlastOffsetPair *)
            malloc(GetOffsetArraySize(lookup_wrap_ptr) *
                   sizeof(BlastOffsetPair));
        BOOST_REQUIRE(offset_pairs != NULL);

        // pick the ScanSubject routine

        BlastChooseProteinScanSubject(lookup_wrap_ptr);

    }

    ~AascanTestFixture() 
    {
        query_blk = BlastSequenceBlkFree(query_blk);
        subject_blk = BlastSequenceBlkFree(subject_blk);
        lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
        lookup_options = LookupTableOptionsFree(lookup_options);
        score_options = BlastScoringOptionsFree(score_options);
        sbp = BlastScoreBlkFree(sbp);
        sfree(offset_pairs);
    }
};

BOOST_FIXTURE_TEST_SUITE( aascan, AascanTestFixture )

BOOST_AUTO_TEST_CASE(ScanOffsetTest)
{
    Int4 query_length = query_blk->length;
    Int4 subject_length = subject_blk->length;
    Int4 hits;
    Uint4 s_off;
    Int4 scan_range[3];

    s_off = 0;

    BOOST_REQUIRE(lookup_wrap_ptr->lut_type == eAaLookupTable);
    BlastAaLookupTable *lut = (BlastAaLookupTable *)(lookup_wrap_ptr->lut);
    TAaScanSubjectFunction scansub = 
                    (TAaScanSubjectFunction)(lut->scansub_callback);
    BOOST_REQUIRE(scansub != NULL);

    scan_range[0] = 0;
    scan_range[1] = 0; 
    scan_range[2] = subject_blk->length - lut->word_length;

    while (scan_range[1] < scan_range[2])
    {
        hits = scansub(lookup_wrap_ptr,
                subject_blk,
                offset_pairs,
                GetOffsetArraySize(lookup_wrap_ptr),
                scan_range);

        // check number of reported hits
        BOOST_REQUIRE(hits <= GetOffsetArraySize(lookup_wrap_ptr));

        if (!hits)
            continue;

        // sort the subject offsets into ascending order,
        // using the query offsets as tiebreakers

        qsort(offset_pairs, hits, sizeof(BlastOffsetPair), compare_offsets);

        // verify that the first offsets in each
        // list pick up where the last ScanSubject
        // call left off, without repetition.
        //
        // The following assumes that all query offsets 
        // for a given subject offset will be collected
        // in a single call to AaScanSubject. 

        if (s_off)
            BOOST_REQUIRE(offset_pairs[0].qs_offsets.s_off > s_off);

        // verify that 
        //   - all offsets are inside their respective sequences
        //   - the subject and query offsets increase monotonically
        //          (i.e. no query-subject pair is repeated)

        for (int i = 1; i < hits; i++)
        {
            BOOST_REQUIRE(offset_pairs[i].qs_offsets.q_off < 
                           (Uint4)(query_length-2));
            BOOST_REQUIRE(offset_pairs[i].qs_offsets.s_off < 
                           (Uint4)(subject_length-2));

            if (offset_pairs[i].qs_offsets.s_off == 
                offset_pairs[i-1].qs_offsets.s_off)
            {
                BOOST_REQUIRE(offset_pairs[i].qs_offsets.q_off >
                               offset_pairs[i-1].qs_offsets.q_off);
            }
            else
            {
                BOOST_REQUIRE(offset_pairs[i].qs_offsets.s_off > 
                               offset_pairs[i-1].qs_offsets.s_off);
            }
        }

        s_off = offset_pairs[hits-1].qs_offsets.s_off;
        BOOST_REQUIRE((Int4)s_off < scan_range[1]);
    }
}
            
BOOST_AUTO_TEST_CASE(ScanMaxHitsTest)
{
    Int4 hits, found_hits, expected_hits;
    Int4 new_max_size;
    Int4 scan_range[3];

    found_hits = expected_hits = 0;

    BOOST_REQUIRE(lookup_wrap_ptr->lut_type == eAaLookupTable);
    BlastAaLookupTable *lut = (BlastAaLookupTable *)(lookup_wrap_ptr->lut);
    TAaScanSubjectFunction scansub = 
                    (TAaScanSubjectFunction)(lut->scansub_callback);
    BOOST_REQUIRE(scansub != NULL);

    // Verify that the number of collected hits does
    // not change if the hit list size changes

    scan_range[0] = 0;
    scan_range[1] = 0;
    scan_range[2] = subject_blk->length - lut->word_length;

    while (scan_range[1] < scan_range[2])
    {
        hits = scansub(lookup_wrap_ptr,
                subject_blk,
                offset_pairs,
                GetOffsetArraySize(lookup_wrap_ptr),
                scan_range);

        BOOST_REQUIRE(hits <= GetOffsetArraySize(lookup_wrap_ptr));
        expected_hits += hits;
    }

    scan_range[0] = 0;
    scan_range[1] = 0;
    scan_range[2] = subject_blk->length - lut->word_length;
    new_max_size = MAX(GetOffsetArraySize(lookup_wrap_ptr)/3,
            ((BlastAaLookupTable *)(lookup_wrap_ptr->lut))->longest_chain);

    while (scan_range[1] < scan_range[2]) 
    {
        hits = scansub(lookup_wrap_ptr,
                subject_blk,
                offset_pairs,
                new_max_size,
                scan_range);
        BOOST_REQUIRE(hits <= new_max_size);
        found_hits += hits;
    }

    BOOST_REQUIRE_EQUAL(found_hits, expected_hits);
}

BOOST_AUTO_TEST_CASE(SkipMaskedRanges)
{
    Int4 subject_length = subject_blk->length;
    Int4 hits;
    Int4 found_hits, expected_hits;
    Int4 scan_range[3];

    found_hits = expected_hits = 0;

    BOOST_REQUIRE(lookup_wrap_ptr->lut_type == eAaLookupTable);
    BlastAaLookupTable *lut = (BlastAaLookupTable *)(lookup_wrap_ptr->lut);
    TAaScanSubjectFunction scansub = 
                    (TAaScanSubjectFunction)(lut->scansub_callback);
    BOOST_REQUIRE(scansub != NULL);

    SSeqRange ranges2scan[] = { {0, 501}, {700, 1001}, {subject_length, subject_length} };
    const size_t kNumRanges = (sizeof(ranges2scan)/sizeof(*ranges2scan));
    BlastSeqBlkSetSeqRanges(subject_blk, ranges2scan, kNumRanges, FALSE, eSoftSubjMasking);

    scan_range[0] = 0;
    scan_range[1] = 0;
    scan_range[2] = ranges2scan[0].right - lut->word_length;

    while (scan_range[1] < scan_range[2]) 
    {
        hits = scansub(lookup_wrap_ptr,
                subject_blk,
                offset_pairs,
                GetOffsetArraySize(lookup_wrap_ptr),
                scan_range);

        BOOST_REQUIRE(hits <= GetOffsetArraySize(lookup_wrap_ptr));
        found_hits += hits;

        // Ensure that hits fall in the subject's "approved" ranges
        for (int i = 0; i < hits; i++) {
            const Uint4 s_off = offset_pairs[i].qs_offsets.s_off;
            bool hit_found = FALSE;
            for (size_t j = 0; j < kNumRanges; j++) {
                if ( s_off >= (Uint4)ranges2scan[j].left && 
                     s_off <  (Uint4)ranges2scan[j].right ) {
                    hit_found = TRUE;
                    break;
                }
            }
            BOOST_REQUIRE( hit_found );
        }
    }
}

BOOST_AUTO_TEST_CASE(ScanCheckScores)
{
    Int4 query_length = query_blk->length;
    Int4 subject_length = subject_blk->length;
    Int4 hits;
    Int4 found_hits, expected_hits;
    Int4 scan_range[3];

    found_hits = expected_hits = 0;

    BOOST_REQUIRE(lookup_wrap_ptr->lut_type == eAaLookupTable);
    BlastAaLookupTable *lut = (BlastAaLookupTable *)(lookup_wrap_ptr->lut);
    TAaScanSubjectFunction scansub = 
                    (TAaScanSubjectFunction)(lut->scansub_callback);
    BOOST_REQUIRE(scansub != NULL);

    // verify the list does not contain any query-subject
    // pairs which do not belong there. 

    scan_range[0] = 0;
    scan_range[1] = 0;
    scan_range[2] = subject_blk->length - lut->word_length;

    while (scan_range[1] < scan_range[2])
    {
        hits = scansub(lookup_wrap_ptr,
                subject_blk,
                offset_pairs,
                GetOffsetArraySize(lookup_wrap_ptr),
                scan_range);

        BOOST_REQUIRE(hits <= GetOffsetArraySize(lookup_wrap_ptr));
        found_hits += hits;

        for (int i = 0; i < hits; i++)
        {
            Uint1 *qres = query_blk->sequence + offset_pairs[i].qs_offsets.q_off;
            Uint1 *sres = 
                subject_blk->sequence + offset_pairs[i].qs_offsets.s_off;
            Int4 score = sbp->matrix->data[qres[0]][sres[0]] +
                         sbp->matrix->data[qres[1]][sres[1]] +
                         sbp->matrix->data[qres[2]][sres[2]];
            Boolean exact = (sres[0] == qres[0]) &&
                            (sres[1] == qres[1]) &&
                            (sres[2] == qres[2]);

            BOOST_REQUIRE(exact || score >= BLAST_WORD_THRESHOLD_BLASTP);
        }
    }
                    
    // The above proved that no incorrect hits made it 
    // into the hit list. Next, attempt to prove that
    // all *expected* hits did make it to the hit list, i.e.
    // the much more important condition that good hits
    // are not left out. We'll try to prove a lesser
    // condition, that exhaustive search for hits through
    // the query and subject sequences finds the same
    // number of high-scoring hits as was found by the 
    // ScanSubject routine.
// 
// XXX Does this also prove the stronger condition 
// when combined with previous unit tests? Handwaving
// proof: suppose the number of hits matches but the
    // ScanSubject routine missed a hit. That would mean
    // either a hit was repeated (previous test showed this
    // didn't happen) or a low-scoring hit was stored by 
    // mistake (previous loop showed this didn't happen), or
    // exhaustive search missed a hit (impossible, or it 
    // wouldn't be exhaustive). QED?

    for (int i = 0; i < (query_length - 2); i++)
    {
        Uint1 *w = query_blk->sequence;
        Uint1 *s = subject_blk->sequence;
        Int4 *p0 = sbp->matrix->data[w[i]];
        Int4 *p1 = sbp->matrix->data[w[i+1]];
        Int4 *p2 = sbp->matrix->data[w[i+2]];

        for (int j = 0; j < (subject_length - 2); j++)
        {
            Int4 score = p0[s[j]] + p1[s[j+1]] + p2[s[j+2]];
            Uint1 different = (w[i] ^ s[j]) | 
                              (w[i+1] ^ s[j+1]) |
                              (w[i+2] ^ s[j+2]);
            if (!different || score >= BLAST_WORD_THRESHOLD_BLASTP)
                expected_hits++;
        }
    }

    BOOST_REQUIRE_EQUAL(found_hits, expected_hits);
}

BOOST_AUTO_TEST_SUITE_END()
#endif
