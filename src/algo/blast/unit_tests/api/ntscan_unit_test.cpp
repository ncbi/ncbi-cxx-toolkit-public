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
*   Nucleotide subject scan unit tests
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
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/blast_nascan.h>
#include <algo/blast/core/na_ungapped.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/lookup_util.h>

#include "test_objmgr.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
static Uint1 template_11_16[] =     {1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1};
static Uint1 template_11_18[] =     {1,0,1,1,0,1,1,0,0,1,0,1,1,0,1,1,0,1};
static Uint1 template_11_21[] =     {1,0,0,1,0,1,1,0,0,1,0,1,1,0,0,1,0,1,1,0,1};
static Uint1 template_11_16_opt[] = {1,1,1,0,0,1,0,1,1,0,1,1,0,1,1,1};
static Uint1 template_11_18_opt[] = {1,1,1,0,1,0,0,1,0,1,1,0,0,1,0,1,1,1};
static Uint1 template_11_21_opt[] = {1,1,1,0,1,0,0,1,0,1,0,0,0,1,0,0,1,0,1,1,1};

static Uint1 template_12_16[] =     {1,1,1,1,1,0,1,1,0,1,1,0,1,1,0,1};
static Uint1 template_12_18[] =     {1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1};
static Uint1 template_12_21[] =     {1,0,0,1,0,1,1,0,1,1,0,1,1,0,0,1,0,1,1,0,1};
static Uint1 template_12_16_opt[] = {1,1,1,0,1,1,0,1,1,0,1,1,0,1,1,1};
static Uint1 template_12_18_opt[] = {1,1,1,0,1,0,1,1,0,0,1,0,1,1,0,1,1,1};
static Uint1 template_12_21_opt[] = {1,1,1,0,1,0,0,1,0,1,1,0,0,1,0,0,1,0,1,1,1};

#define TINY_GI 80982444
#define SM_GI 1945386
#define MED_GI 19572546
#define LG_GI 39919153
#define SUBJECT_GI 39103910

#define NULL_NUCL_SENTINEL 0xf

struct TestFixture {
    BLAST_SequenceBlk *query_blk;
    BlastQueryInfo* query_info;
    BLAST_SequenceBlk *subject_blk;
    BlastScoreBlk *sbp;
    BlastSeqLoc* lookup_segments;
    LookupTableWrap* lookup_wrap_ptr;
    BlastOffsetPair *offset_pairs;
    EBlastProgramType program_number;
    BlastInitialWordOptions* word_options;
    BlastHitSavingOptions* hitsaving_options;
    BlastExtensionOptions* ext_options;

    TestFixture() {
        query_blk = NULL;
        query_info = NULL;
        sbp = NULL;
        subject_blk = NULL;
        lookup_segments = NULL;
        lookup_wrap_ptr = NULL;
        offset_pairs = NULL;
        program_number = eBlastTypeBlastn;
        word_options = NULL;
        hitsaving_options = NULL;
        ext_options = NULL;
    }

    void SetUpQuery(Uint4 query_gi, ENa_strand strand)
    {
        char buf[64];
        Int4 status;
        // load the query
        sprintf(buf, "gi|%d", query_gi);
        CSeq_id id(buf);
        
        auto_ptr<SSeqLoc> ssl(CTestObjMgr::Instance().CreateSSeqLoc(id,strand));

        SBlastSequence sequence(
                    GetSequence(*ssl->seqloc,
                                eBlastEncodingNucleotide,
                                ssl->scope,
                                strand,
                                eSentinels));

        // create the sequence block. The size to pass in
        // must not include the sentinel bytes on either
        // end of the sequence

        query_blk = NULL;
        status = BlastSeqBlkNew(&query_blk);
        BOOST_REQUIRE_EQUAL(0, status);
        status = BlastSeqBlkSetSequence(query_blk, sequence.data.release(),
                               sequence.length - 2);
        BOOST_REQUIRE_EQUAL(0, status);

        BOOST_REQUIRE(query_blk != NULL);
        BOOST_REQUIRE(query_blk->sequence != NULL);
        BOOST_REQUIRE(query_blk->length > 0);

        BOOST_REQUIRE(query_blk != NULL);
        BOOST_REQUIRE(query_blk->sequence[0] != NULL_NUCL_SENTINEL);
        BOOST_REQUIRE(query_blk->sequence[query_blk->length - 1] != 
                                                NULL_NUCL_SENTINEL);
        BOOST_REQUIRE(query_blk->sequence_start[0] == NULL_NUCL_SENTINEL);
        BOOST_REQUIRE(query_blk->sequence_start[query_blk->length + 1] == 
                                                NULL_NUCL_SENTINEL);
        BOOST_REQUIRE_EQUAL(0, (int)query_blk->num_seq_ranges);

        query_info = BlastQueryInfoNew(program_number, 1);


        // indicate which regions of the query to index (handle
        // both strands separately)

        if (strand == eNa_strand_both) {
            const int kStrandLength = (query_blk->length - 1)/2;
            BlastSeqLocNew(&lookup_segments, 0, kStrandLength-1);
            BlastSeqLocNew(&lookup_segments, kStrandLength + 1, 
                                            query_blk->length - 1);
            query_info->contexts[0].query_offset = 0;
            query_info->contexts[0].query_length = kStrandLength;
            query_info->contexts[1].query_offset = kStrandLength + 1;
            query_info->contexts[1].query_length = kStrandLength;
        }
        else {
            BlastSeqLocNew(&lookup_segments, 0, query_blk->length - 1);
            BOOST_REQUIRE(eNa_strand_plus);
            query_info->contexts[0].query_offset = 0;
            query_info->contexts[0].query_length = query_blk->length;
            query_info->contexts[1].query_offset = query_blk->length + 1;
            query_info->contexts[1].query_length = 0;
            query_info->contexts[1].is_valid = FALSE;
        }
    }

    void SetUpSubject(Uint4 subject_gi)
    {
        char buf[64];
        Int4 status;

        // load the subject sequence in compressed format

        sprintf(buf, "gi|%d", subject_gi);
        CSeq_id subject_id(buf);
        
        auto_ptr<SSeqLoc>
            subject_ssl(CTestObjMgr::Instance().CreateSSeqLoc(subject_id,
                                                              eNa_strand_plus));

        SBlastSequence subj_sequence(
                    GetSequence(*subject_ssl->seqloc,
                                eBlastEncodingNcbi2na,
                                subject_ssl->scope,
                                eNa_strand_plus, 
                                eNoSentinels));

        // create the sequence block. Retrieve the real
        // sequence length separately, and verify that
        // the number of bytes allocated by GetSequence()
        // is sufficient to hold that many bases

        subject_blk = NULL;
        status = BlastSeqBlkNew(&subject_blk);
        BOOST_REQUIRE_EQUAL(0, status);
        BOOST_REQUIRE(subject_blk != NULL);
        subject_blk->length = sequence::GetLength(*subject_ssl->seqloc,
                                                  subject_ssl->scope);
        status = BlastSeqBlkSetCompressedSequence(subject_blk, 
                                        subj_sequence.data.release());
        BOOST_REQUIRE_EQUAL(0, status);
        BOOST_REQUIRE(subject_blk->sequence != NULL);
        BOOST_REQUIRE(subject_blk->length > 0);
        BOOST_REQUIRE(subject_blk->length / COMPRESSION_RATIO <=
                       (Int4)subj_sequence.length);
        BOOST_REQUIRE_EQUAL(0, (int)subject_blk->num_seq_ranges);
    }

    void SetUpLookupTable(Boolean mb_lookup, 
                          EDiscWordType disco_type, 
                          Int4 disco_size, 
                          Int4 word_size)
    {
        LookupTableOptions* lookup_options;
        BlastScoringOptions* score_options;
        Int4 status;

        // set lookup table options

        status = LookupTableOptionsNew(program_number, &lookup_options);
        BOOST_REQUIRE_EQUAL(0, status);
        status = BLAST_FillLookupTableOptions(lookup_options,
                                     program_number,
                                     mb_lookup,  // megablast
                                     0,          // threshold
                                     word_size); // word size
        BOOST_REQUIRE_EQUAL(0, status);
        
        // get ready to fill in the scoring matrix

        status = BlastScoringOptionsNew(program_number, &score_options);
        BOOST_REQUIRE_EQUAL(0, status);
        status = BLAST_FillScoringOptions(score_options, 
                                 program_number, 
                                 FALSE,                     // greedy
                                 -3,                         // penalty
                                 1,                        // reward
                                 NULL,                      // score matrix
                                 BLAST_GAP_OPEN_NUCL,       // gap open
                                 BLAST_GAP_EXTN_NUCL        // gap extend
                                 );
        BOOST_REQUIRE_EQUAL(0, status);

        // fill in the score block

        BOOST_REQUIRE(query_blk != NULL);
        const double kScalingFactor = 1.0;
        Blast_Message *blast_message = NULL;
        status = BlastSetup_ScoreBlkInit(query_blk, query_info, score_options, 
                                         program_number, &sbp, kScalingFactor,
                                         &blast_message, NULL);
        BOOST_REQUIRE_EQUAL(0, status);
        blast_message = Blast_MessageFree(blast_message);
        
        // set discontiguous megablast (if applicable)

        lookup_options->mb_template_length = disco_size;
        lookup_options->mb_template_type = disco_type;

        // create the lookup table

        QuerySetUpOptions* query_options = NULL;
        BlastQuerySetUpOptionsNew(&query_options);
        status = LookupTableWrapInit(query_blk,
                            lookup_options,
                            query_options,
                            lookup_segments,
                            sbp,
                            &lookup_wrap_ptr,
                            NULL /* RPS Info */,
                            NULL);
        BOOST_REQUIRE_EQUAL(0, status);
        BlastChooseNaExtend(lookup_wrap_ptr);
        query_options = BlastQuerySetUpOptionsFree(query_options);

        // create the hit collection arrays

        offset_pairs = (BlastOffsetPair*)malloc(
                                   GetOffsetArraySize(lookup_wrap_ptr) * 
                                   sizeof(BlastOffsetPair));
        BOOST_REQUIRE(offset_pairs != NULL);

        lookup_options = LookupTableOptionsFree(lookup_options);
        score_options = BlastScoringOptionsFree(score_options);
        BlastInitialWordOptionsNew(program_number, &word_options);
        BlastExtensionOptionsNew(program_number, &ext_options, TRUE);
        BlastHitSavingOptionsNew(program_number, &hitsaving_options, TRUE);
    }

    void SetUpQuerySubjectAndLUT(Boolean mb_lookup, Int4 gi,
                   EDiscWordType disco_type, Int4 disco_size, Int4 word_size)
    {
        SetUpQuery(gi, eNa_strand_plus);
        SetUpSubject(SUBJECT_GI);
        SetUpLookupTable(mb_lookup, disco_type, disco_size, word_size);
    }

    void TearDownQuery() 
    {
        if (query_blk)
            query_blk = BlastSequenceBlkFree(query_blk);
        if (lookup_segments)
            lookup_segments = BlastSeqLocFree(lookup_segments);
        if (query_info)
            query_info = BlastQueryInfoFree(query_info);
    }

    void TearDownSubject() 
    {
        if (subject_blk)
            subject_blk = BlastSequenceBlkFree(subject_blk);
    }

    void TearDownLookupTable() 
    {
        lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
        sfree(offset_pairs);
        if (sbp)
            sbp = BlastScoreBlkFree(sbp);
        if (word_options)
            word_options = BlastInitialWordOptionsFree(word_options);
        if (hitsaving_options)
            hitsaving_options = BlastHitSavingOptionsFree(hitsaving_options);
        if (ext_options)
            ext_options = BlastExtensionOptionsFree(ext_options);
    }

    ~TestFixture()
    {
        TearDownQuery();
        TearDownSubject();
        TearDownLookupTable();
    }

    Int4 RunScanSubject(Int4 *scan_range,
                        Int4 max_hits) 
    {
        BOOST_REQUIRE(lookup_wrap_ptr->lut_type == eSmallNaLookupTable ||
                       lookup_wrap_ptr->lut_type == eMBLookupTable);

        BlastChooseNucleotideScanSubject(lookup_wrap_ptr);
        TNaScanSubjectFunction callback = NULL;
        if (lookup_wrap_ptr->lut_type == eMBLookupTable) {
            BlastMBLookupTable *mb_lt = (BlastMBLookupTable *)
                                                lookup_wrap_ptr->lut;
            callback = (TNaScanSubjectFunction)mb_lt->scansub_callback;
        }
        else {
            BlastSmallNaLookupTable *na_lt = (BlastSmallNaLookupTable *)
                                       lookup_wrap_ptr->lut;
            callback = (TNaScanSubjectFunction)na_lt->scansub_callback;
        }
        BOOST_REQUIRE(callback != NULL);
        return callback(lookup_wrap_ptr, subject_blk, 
                        offset_pairs, max_hits, scan_range);
    }

    // Gets called first
    void ScanOffsetTestCore(EDiscWordType disco_type)
    {
        Int4 query_bases, subject_bases;
        Int4 scan_range[2];
        Int4 last_lut_word_end;
        Int4 bases_per_lut_word;
        Int4 hits;
        Uint4 last_s_off = 0;
        BlastSmallNaLookupTable *na_lt = NULL;
        BlastMBLookupTable *mb_lt = NULL;
        Boolean discontig = FALSE;

        scan_range[0] = 0;

        BOOST_REQUIRE(query_blk != NULL);
        BOOST_REQUIRE(subject_blk != NULL);
        BOOST_REQUIRE(lookup_wrap_ptr != NULL);
        BOOST_REQUIRE(offset_pairs != NULL);
        BOOST_REQUIRE(lookup_segments != NULL);

        subject_bases = subject_blk->length;
        query_bases = query_blk->length;

        if (lookup_wrap_ptr->lut_type == eMBLookupTable) {
            mb_lt = (BlastMBLookupTable *)lookup_wrap_ptr->lut;
            bases_per_lut_word = mb_lt->lut_word_length;
            discontig = mb_lt->discontiguous;
            //mb_lt->scan_step = 1;

            last_lut_word_end = subject_bases;
            if (discontig) {
                scan_range[1] = subject_bases - mb_lt->template_length;
            }
            else {
                scan_range[1] = subject_bases - bases_per_lut_word;
            }
        }
        else {
            na_lt = (BlastSmallNaLookupTable *)lookup_wrap_ptr->lut;
            bases_per_lut_word = na_lt->lut_word_length;
            scan_range[1] = subject_bases - bases_per_lut_word;
            last_lut_word_end = subject_bases - bases_per_lut_word;
        }

        while (scan_range[0] <= scan_range[1])
        {
            hits = RunScanSubject(scan_range,
                                  GetOffsetArraySize(lookup_wrap_ptr));

            // check number of reported hits
            BOOST_REQUIRE(hits <= GetOffsetArraySize(lookup_wrap_ptr));

            // verify that the first offsets in each
            // list pick up where the last ScanSubject
            // call left off, without repeated subject
            // offsets

            if (!hits)
                continue;

            if (last_s_off)
                BOOST_REQUIRE(offset_pairs[0].qs_offsets.s_off > last_s_off); 

            // verify that 
            //   - the offset recovered from the lookup table is in
            //       the interval [0,query_size-bases_per_word]
            //   - no query-subject pair is repeated. This involves
            //       verifying that subject offsets increase monotonically
            //       and, for equal subject offsets, the query offsets
            //       either increase (blastn) or decrease (megablast)
            //       monotonically
            //       Exception: discontiguous megablast with two templates
            //       is allowed to have nondecreasing query offsets

            for (int i = 1; i < hits; i++)
            {
                BOOST_REQUIRE(offset_pairs[i].qs_offsets.q_off <= 
                               (Uint4)(query_bases - bases_per_lut_word) &&
                               ((int)offset_pairs[i].qs_offsets.q_off) >= 0);
                BOOST_REQUIRE(offset_pairs[i].qs_offsets.s_off < 
                               (Uint4)subject_bases); 

                if (offset_pairs[i].qs_offsets.s_off == 
                    offset_pairs[i-1].qs_offsets.s_off)
                {
                    if (mb_lt) {
                        if (disco_type != eMBWordTwoTemplates) {
                            BOOST_REQUIRE(offset_pairs[i].qs_offsets.q_off < 
                                           offset_pairs[i-1].qs_offsets.q_off);
                        }
                    }
                    else {
                        BOOST_REQUIRE(offset_pairs[i].qs_offsets.q_off > 
                                       offset_pairs[i-1].qs_offsets.q_off);
                    }
                }
                else
                {
                    BOOST_REQUIRE(offset_pairs[i].qs_offsets.s_off > 
                                   offset_pairs[i-1].qs_offsets.s_off);
                }
            }

            last_s_off = offset_pairs[hits-1].qs_offsets.s_off;
        }
    }
                
    // Gets called third
    void ScanMaxHitsTestCore(void)
    {
        Int4 subject_bases;
        Int4 hits, found_hits, expected_hits;
        Int4 scan_range[2];
        Int4 last_lut_word_end;
        Int4 new_max_size;
        BlastSmallNaLookupTable *na_lt = NULL;
        BlastMBLookupTable *mb_lt = NULL;
        Boolean discontig = FALSE;

        scan_range[0] = 0;
        found_hits = expected_hits = 0;

        BOOST_REQUIRE(query_blk != NULL);
        BOOST_REQUIRE(subject_blk != NULL);
        BOOST_REQUIRE(lookup_wrap_ptr != NULL);
        BOOST_REQUIRE(offset_pairs != NULL);
        BOOST_REQUIRE(lookup_segments != NULL);

        subject_bases = subject_blk->length;

        if (lookup_wrap_ptr->lut_type == eMBLookupTable) {
            mb_lt = (BlastMBLookupTable *)lookup_wrap_ptr->lut;
            discontig = mb_lt->discontiguous;
            //mb_lt->scan_step = 1;

            last_lut_word_end = subject_bases;
            if (discontig) {
                scan_range[1] = subject_bases - mb_lt->template_length;
            }
            else {
                scan_range[1] = subject_bases - mb_lt->lut_word_length;
            }
        }
        else {
            na_lt = (BlastSmallNaLookupTable *)lookup_wrap_ptr->lut;
            scan_range[1] = subject_bases - na_lt->lut_word_length;
            last_lut_word_end = subject_bases - na_lt->lut_word_length;
        }

        while (scan_range[0] <= scan_range[1])
        {
            hits = RunScanSubject(scan_range,
                                  GetOffsetArraySize(lookup_wrap_ptr));
            BOOST_REQUIRE(hits <= GetOffsetArraySize(lookup_wrap_ptr));
            expected_hits += hits;
        }

        // Verify that the number of collected hits does
        // not change if the hit list size changes

        scan_range[0] = 0;
        if (mb_lt)
            new_max_size = MAX(GetOffsetArraySize(lookup_wrap_ptr)/5,
                               mb_lt->longest_chain);
        else
            new_max_size = MAX(GetOffsetArraySize(lookup_wrap_ptr)/5,
                               na_lt->longest_chain);

        while (scan_range[0] <= scan_range[1])
        {
            hits = RunScanSubject(scan_range,
                                  new_max_size);
            BOOST_REQUIRE(hits <= new_max_size);
            found_hits += hits;
        }

        BOOST_REQUIRE_EQUAL(found_hits, expected_hits);
    }

    // Gets called second
    void ScanCheckHitsCore(EDiscWordType disco_type)
    {
        Int4 subject_bases;
        Int4 hits, found_hits, expected_hits;
        Int4 scan_range[2];
        Int4 last_lut_word_end;
        Int4 bases_per_lut_word;
        BlastSmallNaLookupTable *na_lt = NULL;
        BlastMBLookupTable *mb_lt = NULL;
        Boolean discontig = FALSE;

        scan_range[0] = 0;
        found_hits = expected_hits = 0;

        BOOST_REQUIRE(query_blk != NULL);
        BOOST_REQUIRE(subject_blk != NULL);
        BOOST_REQUIRE(lookup_wrap_ptr != NULL);
        BOOST_REQUIRE(offset_pairs != NULL);
        BOOST_REQUIRE(lookup_segments != NULL);

        subject_bases = subject_blk->length;

        if (lookup_wrap_ptr->lut_type == eMBLookupTable) {
            mb_lt = (BlastMBLookupTable *)lookup_wrap_ptr->lut;
            bases_per_lut_word = mb_lt->lut_word_length;
            discontig = mb_lt->discontiguous;
            //mb_lt->scan_step = 1;

            last_lut_word_end = subject_bases;
            if (discontig) {
                scan_range[1] = subject_bases - mb_lt->template_length;
            }
            else {
                scan_range[1] = subject_bases - bases_per_lut_word;
            }
        }
        else {
            na_lt = (BlastSmallNaLookupTable *)lookup_wrap_ptr->lut;
            bases_per_lut_word = na_lt->lut_word_length;
            scan_range[1] = subject_bases - bases_per_lut_word;
            last_lut_word_end = subject_bases - bases_per_lut_word;
        }

        while (scan_range[0] <= scan_range[1])
        {
            hits = RunScanSubject(scan_range,
                                  GetOffsetArraySize(lookup_wrap_ptr));
            BOOST_REQUIRE(hits <= GetOffsetArraySize(lookup_wrap_ptr));
            found_hits += hits;

            for (int i = 0; i < hits; i++)
            {
                Uint4 query_word = 0;
                Uint4 query_word2 = 0;
                Uint4 subject_word = 0;
                Uint4 subject_word2 = 0;
                Int4 s_index, s_byte;
                Int4 j;
                Uint1 *q = query_blk->sequence + 
                             offset_pairs[i].qs_offsets.q_off;

                if (discontig) {
                    Uint1 *disco_template = NULL;
                    Uint1 *disco_template2 = NULL;
                    Int4 template_size = 0;

                    switch (mb_lt->template_type) {
                    case eDiscTemplate_11_16_Coding:
                        disco_template = template_11_16;
                        disco_template2 = template_11_16_opt;
                        template_size = 16;
                        break;
                    case eDiscTemplate_11_18_Coding:
                        disco_template = template_11_18;
                        disco_template2 = template_11_18_opt;
                        template_size = 18;
                        break;
                    case eDiscTemplate_11_21_Coding:
                        disco_template = template_11_21;
                        disco_template2 = template_11_21_opt;
                        template_size = 21;
                        break;
                    case eDiscTemplate_11_16_Optimal:
                        disco_template = template_11_16_opt;
                        template_size = 16;
                        break;
                    case eDiscTemplate_11_18_Optimal:
                        disco_template = template_11_18_opt;
                        template_size = 18;
                        break;
                    case eDiscTemplate_11_21_Optimal:
                        disco_template = template_11_21_opt;
                        template_size = 21;
                        break;
                    case eDiscTemplate_12_16_Coding:
                        disco_template = template_12_16;
                        disco_template2 = template_12_16_opt;
                        template_size = 16;
                        break;
                    case eDiscTemplate_12_18_Coding:
                        disco_template = template_12_18;
                        disco_template2 = template_12_18_opt;
                        template_size = 18;
                        break;
                    case eDiscTemplate_12_21_Coding:
                        disco_template = template_12_21;
                        disco_template2 = template_12_21_opt;
                        template_size = 21;
                        break;
                    case eDiscTemplate_12_16_Optimal:
                        disco_template = template_12_16_opt;
                        template_size = 16;
                        break;
                    case eDiscTemplate_12_18_Optimal:
                        disco_template = template_12_18_opt;
                        template_size = 18;
                        break;
                    case eDiscTemplate_12_21_Optimal:
                        disco_template = template_12_21_opt;
                        template_size = 21;
                        break;
                    default:
                        break;
                    }

                    s_index = offset_pairs[i].qs_offsets.s_off;
                    for (j = 0; j < template_size; j++, s_index++) {
                        if (disco_template[j] == 1) {
                            query_word = (query_word << 2) | q[j];
                            s_byte = subject_blk->sequence[ s_index / 
                                                        COMPRESSION_RATIO];
                            subject_word = (subject_word << 2) |
                                    ((s_byte >> (2 * (COMPRESSION_RATIO - 1 -
                                    (s_index % COMPRESSION_RATIO)))) & 0x3);
                        }
                    }

                    if (disco_type == eMBWordTwoTemplates) {
                        s_index = offset_pairs[i].qs_offsets.s_off;
                        for (j = 0; j < template_size; j++, s_index++) {
                            if (disco_template2[j] == 1) {
                                query_word2 = (query_word2 << 2) | q[j];
                                s_byte = subject_blk->sequence[ s_index / 
                                                            COMPRESSION_RATIO];
                                subject_word2 = (subject_word2 << 2) |
                                      ((s_byte >> (2 * (COMPRESSION_RATIO - 1 -
                                      (s_index % COMPRESSION_RATIO)))) & 0x3);
                            }
                        }
                    }
                }
                else {

                    s_index = offset_pairs[i].qs_offsets.s_off;
                    for (j = 0; j < bases_per_lut_word; j++, s_index++) {
                        query_word = (query_word << 2) | q[j];
                        s_byte = subject_blk->sequence[ s_index / 
                                                    COMPRESSION_RATIO];
                        subject_word = (subject_word << 2) |
                                ((s_byte >> (2 * (COMPRESSION_RATIO - 1 -
                                (s_index % COMPRESSION_RATIO)))) & 0x3);
                    }
                }
                if (disco_type == eMBWordTwoTemplates)
                    BOOST_REQUIRE(query_word == subject_word ||
                                   query_word2 == subject_word2);
                else
                    BOOST_REQUIRE_EQUAL(query_word, subject_word);
            }
        }
    }

    // Called fourth
    void SkipMaskedRangesCore(void)
    {
        Int2 retval = 0;
        const Int4 subject_bases = subject_blk->length;

        BOOST_REQUIRE(query_blk != NULL);
        BOOST_REQUIRE(subject_blk != NULL);
        BOOST_REQUIRE(lookup_wrap_ptr != NULL);
        BOOST_REQUIRE(offset_pairs != NULL);
        BOOST_REQUIRE(lookup_segments != NULL);

        SSeqRange ranges2scan[] = { {0, 501}, {700, 1001} , {subject_bases, subject_bases}};
        const size_t kNumRanges = (sizeof(ranges2scan)/sizeof(*ranges2scan));
        BlastSeqBlkSetSeqRanges(subject_blk, ranges2scan, kNumRanges, FALSE, DB_MASK_SOFT);

        BlastHitSavingParameters* hit_params = NULL;
        retval = BlastHitSavingParametersNew(program_number, hitsaving_options,
                                             sbp, query_info, subject_bases, 
                                             &hit_params);
        BOOST_REQUIRE_EQUAL(0, retval);

        BlastInitialWordParameters* word_params = NULL;
        retval = BlastInitialWordParametersNew(program_number, word_options,
                                               hit_params, lookup_wrap_ptr,
                                               sbp, query_info, subject_bases,
                                               &word_params);
        BOOST_REQUIRE_EQUAL(0, retval);

        Blast_ExtendWord* ewp = NULL;
        retval = BlastExtendWordNew(query_blk->length, word_params, &ewp);
        BOOST_REQUIRE_EQUAL(0, retval);

        BlastInitHitList* init_hitlist = BLAST_InitHitListNew();
        BlastUngappedStats ungapped_stats = {0,};
        retval = BlastNaWordFinder(subject_blk, query_blk, query_info,
                                   lookup_wrap_ptr, sbp->matrix->data,
                                   word_params, ewp, offset_pairs,
                                   GetOffsetArraySize(lookup_wrap_ptr),
                                   init_hitlist, &ungapped_stats);
        BOOST_REQUIRE_EQUAL(0, retval);

        // Now for the tests...
        for (int i = 0; i < init_hitlist->total; i++) {
            const BlastInitHSP& init_hsp = init_hitlist->init_hsp_array[i];
            const Uint4 s_off = init_hsp.offsets.qs_offsets.s_off;
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

        hit_params = BlastHitSavingParametersFree(hit_params);
        word_params = BlastInitialWordParametersFree(word_params);
        ewp = BlastExtendWordFree(ewp);
        init_hitlist = BLAST_InitHitListFree(init_hitlist);
    }
};

BOOST_FIXTURE_TEST_SUITE( ntscan, TestFixture )

BOOST_AUTO_TEST_CASE( DiscontigTwoSubjects )
{
    Int4 subject_bases;
    Int4 scan_range[2];
    Int4 last_lut_word_end;
    Int4 hits;
    Int4 i;
    BlastMBLookupTable *mb_lt = NULL;
    const Int4 kWordSize = 12;
    const Int4 kTemplateSize = 21;

    SetUpQuery(555, eNa_strand_both);
    SetUpLookupTable(TRUE, eMBWordTwoTemplates, 
                     kTemplateSize, kWordSize);
    BOOST_REQUIRE(lookup_wrap_ptr->lut_type == eMBLookupTable);
    mb_lt = (BlastMBLookupTable *)lookup_wrap_ptr->lut;
    //mb_lt->scan_step = 1;

    SetUpSubject(313959);
    scan_range[0] = 0;
    subject_bases = subject_blk->length;
    scan_range[1] = subject_bases - mb_lt->template_length;
    last_lut_word_end = subject_bases;

    while (scan_range[0] <= scan_range[1])
    {
        hits = RunScanSubject(scan_range,
                              GetOffsetArraySize(lookup_wrap_ptr));
    }

    TearDownSubject();
    SetUpSubject(271065);  // smaller subject sequence
    scan_range[0] = 0;
    subject_bases = subject_blk->length;
    scan_range[1] = subject_bases - mb_lt->template_length;
    last_lut_word_end = subject_bases;

    while (scan_range[0] <= scan_range[1])
    {
        hits = RunScanSubject(scan_range,
                              GetOffsetArraySize(lookup_wrap_ptr));

        // verify that none of the lookup table hits are 'reused'
        // from the last subject sequence

        for (i = 0; i < hits; i++) {
            BOOST_REQUIRE(offset_pairs[i].qs_offsets.s_off < 
                           (Uint4)subject_bases);
        }
    }
}

#define DECLARE_TEST(name, gi, d_size, d_type, wordsize)                    \
BOOST_AUTO_TEST_CASE( name##ScanOffsetSize##wordsize ) {                    \
    SetUpQuerySubjectAndLUT(TRUE, gi, (EDiscWordType)d_type, d_size, wordsize);\
    ScanOffsetTestCore((EDiscWordType)d_type);                              \
    ScanCheckHitsCore((EDiscWordType)d_type);                               \
    ScanMaxHitsTestCore();                                                  \
    SkipMaskedRangesCore();                                                 \
}

DECLARE_TEST(Tiny, TINY_GI, 0, 0, 4);
DECLARE_TEST(Tiny, TINY_GI, 0, 0, 5);
DECLARE_TEST(Tiny, TINY_GI, 0, 0, 6);
DECLARE_TEST(Tiny, TINY_GI, 0, 0, 7);

DECLARE_TEST(Small, SM_GI, 0, 0, 6);
DECLARE_TEST(Small, SM_GI, 0, 0, 7);
DECLARE_TEST(Small, SM_GI, 0, 0, 8);
DECLARE_TEST(Small, SM_GI, 0, 0, 9);
DECLARE_TEST(Small, SM_GI, 0, 0, 10);

DECLARE_TEST(Medium, MED_GI, 0, 0, 9);
DECLARE_TEST(Medium, MED_GI, 0, 0, 10);
DECLARE_TEST(Medium, MED_GI, 0, 0, 11);
DECLARE_TEST(Medium, MED_GI, 0, 0, 12);
DECLARE_TEST(Medium, MED_GI, 0, 0, 13);
DECLARE_TEST(Medium, MED_GI, 0, 0, 14);
DECLARE_TEST(Medium, MED_GI, 0, 0, 15);
DECLARE_TEST(Medium, MED_GI, 0, 0, 20);

DECLARE_TEST(Large, LG_GI, 0, 0, 11);
DECLARE_TEST(Large, LG_GI, 0, 0, 12);
DECLARE_TEST(Large, LG_GI, 0, 0, 13);
DECLARE_TEST(Large, LG_GI, 0, 0, 15);
DECLARE_TEST(Large, LG_GI, 0, 0, 20);
DECLARE_TEST(Large, LG_GI, 0, 0, 25);
DECLARE_TEST(Large, LG_GI, 0, 0, 28);
DECLARE_TEST(Large, LG_GI, 0, 0, 33);
DECLARE_TEST(Large, LG_GI, 0, 0, 37);

DECLARE_TEST(Disco_Coding_16_, MED_GI, 16, eMBWordCoding, 11)
DECLARE_TEST(Disco_Coding_18_, MED_GI, 18, eMBWordCoding, 11)
DECLARE_TEST(Disco_Coding_21_, MED_GI, 21, eMBWordCoding, 11)
DECLARE_TEST(Disco_Optimal_16_, MED_GI, 16, eMBWordOptimal, 11)
DECLARE_TEST(Disco_Optimal_18_, MED_GI, 18, eMBWordOptimal, 11)
DECLARE_TEST(Disco_Optimal_21_, MED_GI, 21, eMBWordOptimal, 11)

DECLARE_TEST(Disco_2Templ_16_, MED_GI, 16, eMBWordTwoTemplates, 11)
DECLARE_TEST(Disco_2Templ_18_, MED_GI, 18, eMBWordTwoTemplates, 11)
DECLARE_TEST(Disco_2Templ_21_, MED_GI, 21, eMBWordTwoTemplates, 11)

DECLARE_TEST(Disco_Coding_16_, MED_GI, 16, eMBWordCoding, 12)
DECLARE_TEST(Disco_Coding_18_, MED_GI, 18, eMBWordCoding, 12)
DECLARE_TEST(Disco_Coding_21_, MED_GI, 21, eMBWordCoding, 12)
DECLARE_TEST(Disco_Optimal_16_, MED_GI, 16, eMBWordOptimal, 12)
DECLARE_TEST(Disco_Optimal_18_, MED_GI, 18, eMBWordOptimal, 12)
DECLARE_TEST(Disco_Optimal_21_, MED_GI, 21, eMBWordOptimal, 12)

DECLARE_TEST(Disco_2Templ_16_, MED_GI, 16, eMBWordTwoTemplates, 12)
DECLARE_TEST(Disco_2Templ_18_, MED_GI, 18, eMBWordTwoTemplates, 12)
DECLARE_TEST(Disco_2Templ_21_, MED_GI, 21, eMBWordTwoTemplates, 12)

BOOST_AUTO_TEST_SUITE_END()
#endif
