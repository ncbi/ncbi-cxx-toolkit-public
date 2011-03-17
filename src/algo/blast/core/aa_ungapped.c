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
 */

/** @file aa_ungapped.c
 * Functions to iterate over seed hits and perform ungapped extensions.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif                          /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/aa_ungapped.h>
#include <algo/blast/core/blast_aalookup.h>
#include <algo/blast/core/blast_aascan.h>
#include <algo/blast/core/blast_util.h>

/** Scan a subject sequence for word hits and trigger two-hit extensions.
 *
 * @param subject the subject sequence [in]
 * @param query the query sequence [in]
 * @param lookup_wrap the lookup table [in]
 * @param ewp Structure containing the diagonal array
 * @param matrix the substitution matrix [in]
 * @param word_params structure containing per-context cutoff information [in]
 * @param query_info structure containing context ranges [in]
 * @param offset_pairs Array for storing query and subject offsets. [in]
 * @param array_size the number of elements in each offset array [in]
 * @param ungapped_hsps hsps resulting from the ungapped extension [out]
 * @param ungapped_stats Various hit counts. Not filled if NULL [out]
 */
static Int2 s_BlastAaWordFinder_TwoHit(const BLAST_SequenceBlk* subject,
                                const BLAST_SequenceBlk* query,
                                const LookupTableWrap* lookup_wrap,
                                Blast_ExtendWord * ewp,
                                Int4 ** matrix,
                                const BlastInitialWordParameters * word_params,
                                BlastQueryInfo * query_info,
                                BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                                Int4 array_size,
                                BlastInitHitList* ungapped_hsps, 
                                BlastUngappedStats* ungapped_stats);

/** Scan a subject sequence for word hits and trigger two-hit extensions
 * (specialized for RPS blast).
 *
 * @param subject the subject sequence [in]
 * @param query the query sequence [in]
 * @param lookup_wrap the lookup table [in]
 * @param ewp Structure containing the diagonal array [in]
 * @param matrix the substitution matrix [in]
 * @param cutoff cutoff score for saving ungapped HSPs [in]
 * @param dropoff x dropoff [in]
 * @param ungapped_hsps hsps resulting from the ungapped extension [out]
 * @param ungapped_stats Various hit counts. Not filled if NULL [out]
 */
static Int2 s_BlastRPSWordFinder_TwoHit(const BLAST_SequenceBlk* subject,
                               const BLAST_SequenceBlk* query,
                               const LookupTableWrap* lookup_wrap,
                               Blast_ExtendWord * ewp,
                               Int4 ** matrix,
                               Int4 cutoff,
                               Int4 dropoff,
                               BlastInitHitList* ungapped_hsps, 
                               BlastUngappedStats* ungapped_stats);

/** Scan a subject sequence for word hits and trigger one-hit extensions.
 *
 * @param subject the subject sequence
 * @param query the query sequence
 * @param lookup_wrap the lookup table
 * @param ewp Structure containing the diagonal array [in]
 * @param matrix the substitution matrix [in]
 * @param word_params structure containing per-context cutoff information [in]
 * @param query_info structure containing context ranges [in]
 * @param offset_pairs Array for storing query and subject offsets. [in]
 * @param array_size the number of elements in each offset array
 * @param ungapped_hsps hsps resulting from the ungapped extensions [out]
 * @param ungapped_stats Various hit counts. Not filled if NULL [out]
 */
static Int2 s_BlastAaWordFinder_OneHit(const BLAST_SequenceBlk* subject,
                              const BLAST_SequenceBlk* query,
                              const LookupTableWrap* lookup_wrap,
                              Blast_ExtendWord * ewp,
                              Int4 ** matrix,
                              const BlastInitialWordParameters * word_params,
                              BlastQueryInfo * query_info,
                              BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                              Int4 array_size,
                              BlastInitHitList* ungapped_hsps, 
                              BlastUngappedStats* ungapped_stats);

/** Scan a subject sequence for word hits and trigger one-hit extensions
 * (spcialized for RPS blast).
 *
 * @param subject the subject sequence
 * @param query the query sequence
 * @param lookup_wrap the lookup table
 * @param ewp Structure containing the diagonal array [in]
 * @param matrix the substitution matrix [in]
 * @param cutoff cutoff score for saving ungapped HSPs [in]
 * @param dropoff x dropoff [in]
 * @param ungapped_hsps hsps resulting from the ungapped extensions [out]
 * @param ungapped_stats Various hit counts. Not filled if NULL [out]
 */
static Int2 s_BlastRPSWordFinder_OneHit(const BLAST_SequenceBlk* subject,
                               const BLAST_SequenceBlk* query,
                               const LookupTableWrap* lookup_wrap,
                               Blast_ExtendWord * ewp,
                               Int4 ** matrix,
                               Int4 cutoff,
                               Int4 dropoff,
                               BlastInitHitList* ungapped_hsps, 
                               BlastUngappedStats* ungapped_stats);

/** Perform a one-hit extension. Beginning at the specified hit,
 * extend to the left, then extend to the right. 
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_off subject offset [in]
 * @param q_off query offset [in]
 * @param dropoff X dropoff parameter [in]
 * @param hsp_q the offset in the query where the HSP begins [out]
 * @param hsp_s the offset in the subject where the HSP begins [out]
 * @param hsp_len the length of the HSP [out]
 * @param word_size number of letters in the initial word hit [in]
 * @param use_pssm TRUE if the scoring matrix is position-specific [in]
 * @param s_last_off the rightmost subject offset examined [out]
 * @return the score of the hsp.
 */
static Int4 s_BlastAaExtendOneHit(Int4 ** matrix,
                         const BLAST_SequenceBlk* subject,
                         const BLAST_SequenceBlk* query,
                         Int4 s_off,
                         Int4 q_off,
                         Int4 dropoff,
                         Int4* hsp_q,
                         Int4* hsp_s,
                         Int4* hsp_len,
                         Int4 word_size,
                         Boolean use_pssm,
                         Int4* s_last_off);
                         
/** Perform a two-hit extension. Given two hits L and R, begin
 * at R and extend to the left. If we do not reach L, abort the extension.
 * Otherwise, begin at R and extend to the right.
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_left_off left subject offset [in]
 * @param s_right_off right subject offset [in]
 * @param q_right_off right query offset [in]
 * @param dropoff X dropoff parameter [in]
 * @param hsp_q the offset in the query where the HSP begins [out]
 * @param hsp_s the offset in the subject where the HSP begins [out]
 * @param hsp_len the length of the HSP [out]
 * @param use_pssm TRUE if the scoring matrix is position-specific [in]
 * @param word_size number of letters in one word [in]
 * @param right_extend set to TRUE if an extension to the right happened [out]
 * @param s_last_off the rightmost subject offset examined [out]
 * @return the score of the hsp.
 */
static Int4 s_BlastAaExtendTwoHit(Int4 ** matrix,
                         const BLAST_SequenceBlk* subject,
                         const BLAST_SequenceBlk* query,
                         Int4 s_left_off,
                         Int4 s_right_off,
                         Int4 q_right_off,
                         Int4 dropoff,
                         Int4* hsp_q,
                         Int4* hsp_s,
                         Int4* hsp_len,
                         Boolean use_pssm,
                         Int4 word_size,
                         Boolean *right_extend,
                         Int4* s_last_off);


Int2 BlastAaWordFinder(BLAST_SequenceBlk * subject,
                       BLAST_SequenceBlk * query,
                       BlastQueryInfo * query_info,
                       LookupTableWrap * lut_wrap,
                       Int4 ** matrix,
                       const BlastInitialWordParameters * word_params,
                       Blast_ExtendWord * ewp,
                       BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                       Int4 offset_array_size,
                       BlastInitHitList * init_hitlist,
                       BlastUngappedStats * ungapped_stats)
{
    Int2 status = 0;

    if (ewp->diag_table->multiple_hits) {
        status = s_BlastAaWordFinder_TwoHit(subject, query,
                                            lut_wrap, ewp,
                                            matrix,
                                            word_params,
                                            query_info,
                                            offset_pairs,
                                            offset_array_size,
                                            init_hitlist, ungapped_stats);
    } else {
        status = s_BlastAaWordFinder_OneHit(subject, query,
                                            lut_wrap, ewp,
                                            matrix,
                                            word_params,
                                            query_info,
                                            offset_pairs,
                                            offset_array_size,
                                            init_hitlist, ungapped_stats);
    }

    Blast_InitHitListSortByScore(init_hitlist);
    return status;
}

Int2 BlastRPSWordFinder(BLAST_SequenceBlk * subject,
                        BLAST_SequenceBlk * query,
                        BlastQueryInfo * query_info,
                        LookupTableWrap * lut_wrap,
                        Int4 ** matrix,
                        const BlastInitialWordParameters * word_params,
                        Blast_ExtendWord * ewp,
                        BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                        Int4 offset_array_size,
                        BlastInitHitList * init_hitlist,
                        BlastUngappedStats * ungapped_stats)
{
    Int2 status = 0;
    BlastUngappedCutoffs *cutoffs;
    Int4 context = subject->oid;

    /* for rpsblast, query contains the concatenated database
       and subject contains one query sequence (or one frame
       of a translated nucleotide sequence). This means the cutoff
       and X-drop value is the same for all hits */

    if (subject->frame != 0) {
        context = subject->oid * NUM_FRAMES +
                        BLAST_FrameToContext(subject->frame, 
                                             eBlastTypeRpsTblastn);
    }
    cutoffs = word_params->cutoffs + context;

    if (ewp->diag_table->multiple_hits) {
        status = s_BlastRPSWordFinder_TwoHit(subject, query,
                                             lut_wrap, ewp,
                                             matrix,
                                             cutoffs->cutoff_score,
                                             cutoffs->x_dropoff,
                                             init_hitlist, ungapped_stats);
    } else {
        status = s_BlastRPSWordFinder_OneHit(subject, query,
                                             lut_wrap, ewp,
                                             matrix,
                                             cutoffs->cutoff_score,
                                             cutoffs->x_dropoff,
                                             init_hitlist, ungapped_stats);
    }

    Blast_InitHitListSortByScore(init_hitlist);
    return status;
}

static Int2
s_BlastRPSWordFinder_TwoHit(const BLAST_SequenceBlk * subject,
                            const BLAST_SequenceBlk * query,
                            const LookupTableWrap * lookup_wrap,
                            Blast_ExtendWord * ewp,
                            Int4 ** matrix,
                            Int4 cutoff,
                            Int4 dropoff,
                            BlastInitHitList * ungapped_hsps,
                            BlastUngappedStats * ungapped_stats)
{
    BlastRPSLookupTable *lookup;
    Int4 wordsize;
    Int4 i, j;
    Int4 hits = 0;
    Int4 totalhits = 0;
    Int4 first_offset = 0;
    Int4 last_offset;
    Int4 score;
    Int4 hsp_q, hsp_s, hsp_len;
    Int4 window;
    Int4 last_hit, s_last_off, diff;
    Int4 diag_offset, diag_coord, diag_mask;
    DiagStruct *diag_array;
    Boolean right_extend;
    Int4 hits_extended = 0;
    BLAST_DiagTable * diag = ewp->diag_table;

    ASSERT(diag != NULL);

    diag_offset = diag->offset;
    diag_array = diag->hit_level_array;

    ASSERT(diag_array);

    diag_mask = diag->diag_mask;
    window = diag->window;

    lookup = (BlastRPSLookupTable *) lookup_wrap->lut;
    wordsize = lookup->wordsize;
    last_offset = subject->length - wordsize;

    while (first_offset <= last_offset) {
        /* scan the subject sequence for hits */
        hits = BlastRPSScanSubject(lookup_wrap, subject, &first_offset);

        totalhits += hits;
        /* for each region of the concatenated database */
        for (i = 0; i < lookup->num_buckets; ++i) {
            RPSBucket *curr_bucket = lookup->bucket_array + i;
            BlastOffsetPair *offset_pairs = curr_bucket->offset_pairs;
            hits = curr_bucket->num_filled;

            /* handle each hit in the neighborhood. Because the neighborhood
               is small and there should be many hits there, proceeding one
               bucket at a time provides maximum reuse of data structures. A
               side benefit is that ungapped extensions also cluster together 
               in the concatenated database, reusing PSSM data as well. */
            for (j = 0; j < hits; ++j) {
                Uint4 query_offset = offset_pairs[j].qs_offsets.q_off;
                Uint4 subject_offset = offset_pairs[j].qs_offsets.s_off;

                /* calculate the diagonal associated with this query-subject
                   pair */

                diag_coord = (query_offset - subject_offset) & diag_mask;

                /* If the reset bit is set, an extension just happened. */
                if (diag_array[diag_coord].flag) {
                    /* If we've already extended past this hit, skip it. */
                    if ((Int4) (subject_offset + diag_offset) <
                        diag_array[diag_coord].last_hit) {
                        continue;
                    }
                    /* Otherwise, start a new hit. */
                    else {
                        diag_array[diag_coord].last_hit =
                            subject_offset + diag_offset;
                        diag_array[diag_coord].flag = 0;
                    }
                }
                /* If the reset bit is cleared, try to start an extension. */
                else {
                    /* find the distance to the last hit on this diagonal */
                    last_hit = diag_array[diag_coord].last_hit - diag_offset;
                    diff = subject_offset - last_hit;

                    if (diff >= window) {
                        /* We are beyond the window for this diagonal; start
                           a new hit */
                        diag_array[diag_coord].last_hit =
                            subject_offset + diag_offset;
                        continue;
                    }

                    /* If the difference is less than the wordsize (i.e.
                       last hit and this hit overlap), give up */

                    if (diff < wordsize) {
                        continue;
                    }

                    /* Extend this pair of hits. The extension to the left
                       must reach the end of the first word in order for
                       extension to the right to proceed. */

                    score = s_BlastAaExtendTwoHit(matrix, subject, query,
                                                  last_hit + wordsize,
                                                  subject_offset, query_offset,
                                                  dropoff, &hsp_q, &hsp_s,
                                                  &hsp_len, TRUE,
                                                  wordsize, &right_extend,
                                                  &s_last_off);

                    ++hits_extended;

                    /* if the hsp meets the score threshold, report it */
                    if (score >= cutoff)
                        BlastSaveInitHsp(ungapped_hsps, hsp_q, hsp_s,
                                         query_offset, subject_offset,
                                         hsp_len, score);

                    /* If an extension to the right happened, reset the last
                       hit so that future hits to this diagonal must start
                       over. */

                    if (right_extend) {
                        diag_array[diag_coord].flag = 1;
                        diag_array[diag_coord].last_hit =
                            s_last_off - (wordsize - 1) + diag_offset;
                    }
                    /* Otherwise, make the present hit into the previous hit
                       for this diagonal */
                    else {
                        diag_array[diag_coord].last_hit =
                            subject_offset + diag_offset;
                    }
                }               /* end else */

            }                   /* end for - done with this batch of hits */
        }                       /* end for - done with this bucket, call
                                   scansubject again */
    }                           /* end while - done with the entire sequence. 
                                 */

    /* increment the offset in the diagonal array */
    Blast_ExtendWordExit(ewp, subject->length);

    Blast_UngappedStatsUpdate(ungapped_stats, totalhits, hits_extended,
                              ungapped_hsps->total);
    return 0;
}

static Int2
s_BlastAaWordFinder_TwoHit(const BLAST_SequenceBlk * subject,
                           const BLAST_SequenceBlk * query,
                           const LookupTableWrap * lookup_wrap,
                           Blast_ExtendWord * ewp,
                           Int4 ** matrix,
                           const BlastInitialWordParameters * word_params,
                           BlastQueryInfo * query_info,
                           BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                           Int4 array_size,
                           BlastInitHitList * ungapped_hsps,
                           BlastUngappedStats * ungapped_stats)
{
    Boolean use_pssm = FALSE;
    Int4 wordsize;
    Int4 i;
    Int4 hits = 0;
    Int4 totalhits = 0;
    Int4 score;
    Int4 hsp_q, hsp_s, hsp_len = 0;
    Int4 window;
    Int4 last_hit, s_last_off, diff;
    Int4 diag_offset, diag_coord, diag_mask;
    DiagStruct *diag_array;
    Boolean right_extend;
    Int4 hits_extended = 0;
    Int4 curr_context;
    BlastUngappedCutoffs *cutoffs;
    BLAST_DiagTable * diag = ewp->diag_table;
    TAaScanSubjectFunction scansub;
    Int4 scan_range[3];

    ASSERT(diag != NULL);

    diag_offset = diag->offset;
    diag_array = diag->hit_level_array;

    ASSERT(diag_array);

    diag_mask = diag->diag_mask;
    window = diag->window;

    if (lookup_wrap->lut_type == eAaLookupTable) {
        BlastAaLookupTable *lookup = 
                        (BlastAaLookupTable *)(lookup_wrap->lut);
        scansub = (TAaScanSubjectFunction)(lookup->scansub_callback);
        wordsize = lookup->word_length;
        use_pssm = lookup->use_pssm;
    }
    else {
        BlastCompressedAaLookupTable *lookup = 
                        (BlastCompressedAaLookupTable *)(lookup_wrap->lut);
        scansub = (TAaScanSubjectFunction)(lookup->scansub_callback);
        wordsize = lookup->word_length;
    }

    scan_range[0] = 0;
    scan_range[1] = subject->seq_ranges[0].left;
    scan_range[2] = subject->seq_ranges[0].right - wordsize;

    if (scan_range[2] < scan_range[1])
        scan_range[2] = scan_range[1];

    while (scan_range[1] <= scan_range[2]) {
        /* scan the subject sequence for hits */
        hits = scansub(lookup_wrap, subject, 
                                  offset_pairs, array_size, scan_range);

        totalhits += hits;
        /* for each hit, */
        for (i = 0; i < hits; ++i) {
            Uint4 query_offset = offset_pairs[i].qs_offsets.q_off;
            Uint4 subject_offset = offset_pairs[i].qs_offsets.s_off;

            /* calculate the diagonal associated with this query-subject pair 
             */

            diag_coord = (query_offset - subject_offset) & diag_mask;

            /* If the reset bit is set, an extension just happened. */
            if (diag_array[diag_coord].flag) {
                /* If we've already extended past this hit, skip it. */
                if ((Int4) (subject_offset + diag_offset) <
                    diag_array[diag_coord].last_hit) {
                    continue;
                }
                /* Otherwise, start a new hit. */
                else {
                    diag_array[diag_coord].last_hit =
                        subject_offset + diag_offset;
                    diag_array[diag_coord].flag = 0;
                }
            }
            /* If the reset bit is cleared, try to start an extension. */
            else {
                /* find the distance to the last hit on this diagonal */
                last_hit = diag_array[diag_coord].last_hit - diag_offset;
                diff = subject_offset - last_hit;

                if (diff >= window) {
                    /* We are beyond the window for this diagonal; start a
                       new hit */
                    diag_array[diag_coord].last_hit =
                        subject_offset + diag_offset;
                    continue;
                }

                /* If the difference is less than the wordsize (i.e. last
                   hit and this hit overlap), give up */

                if (diff < wordsize) {
                    continue;
                }

                /* Extend this pair of hits. The extension to the left must
                   reach the end of the first word in order for extension to 
                   the right to proceed.
                 
                   To use the cutoff and X-drop values appropriate for this
                   extension, the query context must first be found */

                curr_context = BSearchContextInfo(query_offset, query_info);
                cutoffs = word_params->cutoffs + curr_context;
                score = s_BlastAaExtendTwoHit(matrix, subject, query,
                                              last_hit + wordsize,
                                              subject_offset, query_offset,
                                              cutoffs->x_dropoff, 
                                              &hsp_q, &hsp_s,
                                              &hsp_len, use_pssm,
                                              wordsize, &right_extend,
                                              &s_last_off);

                ++hits_extended;

                /* if the hsp meets the score threshold, report it */
                if (score >= cutoffs->cutoff_score)
                    BlastSaveInitHsp(ungapped_hsps, hsp_q, hsp_s,
                                     query_offset, subject_offset, hsp_len,
                                     score);

                /* If an extension to the right happened, reset the last hit
                   so that future hits to this diagonal must start over. */

                if (right_extend) {
                    diag_array[diag_coord].flag = 1;
                    diag_array[diag_coord].last_hit =
                        s_last_off - (wordsize - 1) + diag_offset;
                }
                /* Otherwise, make the present hit into the previous hit for
                   this diagonal */
                else {
                    diag_array[diag_coord].last_hit =
                        subject_offset + diag_offset;
                }
            }                   /* end else */
        }                       /* end for - done with this batch of hits,
                                   call scansubject again. */
    }                           /* end while - done with the entire sequence. 
                                 */

    /* increment the offset in the diagonal array */
    Blast_ExtendWordExit(ewp, subject->length);

    Blast_UngappedStatsUpdate(ungapped_stats, totalhits, hits_extended,
                              ungapped_hsps->total);
    return 0;
}

static Int2 
s_BlastRPSWordFinder_OneHit(const BLAST_SequenceBlk * subject,
                            const BLAST_SequenceBlk * query,
                            const LookupTableWrap * lookup_wrap,
                            Blast_ExtendWord * ewp,
                            Int4 ** matrix,
                            Int4 cutoff,
                            Int4 dropoff,
                            BlastInitHitList * ungapped_hsps,
                            BlastUngappedStats * ungapped_stats)
{
    BlastRPSLookupTable *lookup = NULL;
    Int4 wordsize;
    Int4 hits = 0;
    Int4 totalhits = 0;
    Int4 first_offset = 0;
    Int4 last_offset;
    Int4 hsp_q, hsp_s, hsp_len;
    Int4 s_last_off;
    Int4 i, j;
    Int4 score;
    Int4 diag_offset, diag_coord, diag_mask, diff;
    DiagStruct *diag_array;
    Int4 hits_extended = 0;
    BLAST_DiagTable * diag = ewp->diag_table;

    ASSERT(diag != NULL);

    diag_offset = diag->offset;
    diag_array = diag->hit_level_array;
    ASSERT(diag_array);

    diag_mask = diag->diag_mask;

    lookup = (BlastRPSLookupTable *) lookup_wrap->lut;
    wordsize = lookup->wordsize;
    last_offset = subject->length - wordsize;

    while (first_offset <= last_offset) {
        /* scan the subject sequence for hits */
        hits = BlastRPSScanSubject(lookup_wrap, subject, &first_offset);

        totalhits += hits;
        for (i = 0; i < lookup->num_buckets; i++) {
            RPSBucket *curr_bucket = lookup->bucket_array + i;
            BlastOffsetPair *offset_pairs = curr_bucket->offset_pairs;
            hits = curr_bucket->num_filled;

            /* handle each hit in the neighborhood. Because the neighborhood
               is small and there should be many hits there, proceeding one
               bucket at a time provides maximum reuse of data structures. A
               side benefit is that ungapped extensions also cluster together 
               in the concatenated database, reusing PSSM data as well. */
            for (j = 0; j < hits; ++j) {
                Uint4 query_offset = offset_pairs[j].qs_offsets.q_off;
                Uint4 subject_offset = offset_pairs[j].qs_offsets.s_off;
                diag_coord = (subject_offset - query_offset) & diag_mask;
                diff = subject_offset -
                    (diag_array[diag_coord].last_hit - diag_offset);

                /* do an extension, but only if we have not already extended
                   this far */
                if (diff >= 0) {
                    ++hits_extended;
                    score = s_BlastAaExtendOneHit(matrix, subject, query,
                                                  subject_offset, query_offset,
                                                  dropoff, &hsp_q, &hsp_s,
                                                  &hsp_len, wordsize, TRUE,
                                                  &s_last_off);

                    /* if the hsp meets the score threshold, report it */
                    if (score >= cutoff) {
                        BlastSaveInitHsp(ungapped_hsps, hsp_q, hsp_s,
                                         query_offset, subject_offset,
                                         hsp_len, score);
                    }
                    diag_array[diag_coord].last_hit =
                        s_last_off - (wordsize - 1) + diag_offset;
                }
            }                   /* end for (one bucket) */
        }                       /* end for (all buckets) */
    }                           /* end while */

    /* increment the offset in the diagonal array (no windows used) */
    Blast_ExtendWordExit(ewp, subject->length);

    Blast_UngappedStatsUpdate(ungapped_stats, totalhits, hits_extended,
                              ungapped_hsps->total);
    return 0;
}

static Int2 
s_BlastAaWordFinder_OneHit(const BLAST_SequenceBlk * subject,
                           const BLAST_SequenceBlk * query,
                           const LookupTableWrap * lookup_wrap,
                           Blast_ExtendWord * ewp,
                           Int4 ** matrix,
                           const BlastInitialWordParameters * word_params,
                           BlastQueryInfo * query_info,
                           BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                           Int4 array_size,
                           BlastInitHitList * ungapped_hsps,
                           BlastUngappedStats * ungapped_stats)
{
    Boolean use_pssm = FALSE;
    Int4 wordsize;
    Int4 hits = 0;
    Int4 totalhits = 0;
    Int4 hsp_q, hsp_s, hsp_len;
    Int4 s_last_off;
    Int4 i;
    Int4 score;
    Int4 diag_offset, diag_coord, diag_mask, diff;
    DiagStruct *diag_array;
    Int4 hits_extended = 0;
    BLAST_DiagTable * diag = ewp->diag_table;
    TAaScanSubjectFunction scansub;
    Int4 scan_range[3];

    ASSERT(diag != NULL);

    diag_offset = diag->offset;
    diag_array = diag->hit_level_array;
    ASSERT(diag_array);

    diag_mask = diag->diag_mask;

    if (lookup_wrap->lut_type == eAaLookupTable) {
        BlastAaLookupTable *lookup = 
                        (BlastAaLookupTable *)(lookup_wrap->lut);
        scansub = (TAaScanSubjectFunction)(lookup->scansub_callback);
        wordsize = lookup->word_length;
        use_pssm = lookup->use_pssm;
    }
    else {
        BlastCompressedAaLookupTable *lookup = 
                        (BlastCompressedAaLookupTable *)(lookup_wrap->lut);
        scansub = (TAaScanSubjectFunction)(lookup->scansub_callback);
        wordsize = lookup->word_length;
    }

    scan_range[0] = 0;
    scan_range[1] = subject->seq_ranges[0].left;
    scan_range[2] = subject->seq_ranges[0].right - wordsize;

    while (scan_range[1] <= scan_range[2]) {
        /* scan the subject sequence for hits */
        hits = scansub(lookup_wrap, subject,
                       offset_pairs, array_size, scan_range);

        totalhits += hits;
        /* for each hit, */
        for (i = 0; i < hits; ++i) {
            Uint4 query_offset = offset_pairs[i].qs_offsets.q_off;
            Uint4 subject_offset = offset_pairs[i].qs_offsets.s_off;
            diag_coord = (subject_offset - query_offset) & diag_mask;
            diff = subject_offset -
                (diag_array[diag_coord].last_hit - diag_offset);

            /* do an extension, but only if we have not already extended this 
               far */
            if (diff >= 0) {
                Int4 curr_context = BSearchContextInfo(query_offset, 
                                                       query_info);
                BlastUngappedCutoffs *cutoffs = word_params->cutoffs + 
                                                        curr_context;
                score = s_BlastAaExtendOneHit(matrix, subject, query,
                                              subject_offset, query_offset,
                                              cutoffs->x_dropoff, 
                                              &hsp_q, &hsp_s, &hsp_len,
                                              wordsize, use_pssm, &s_last_off);

                /* if the hsp meets the score threshold, report it */
                if (score >= cutoffs->cutoff_score) {
                    BlastSaveInitHsp(ungapped_hsps, hsp_q, hsp_s,
                                     query_offset, subject_offset, hsp_len,
                                     score);
                }
                diag_array[diag_coord].last_hit =
                    s_last_off - (wordsize - 1) + diag_offset;
                ++hits_extended;
            }
        }                       /* end for */
    }                           /* end while */

    /* increment the offset in the diagonal array (no windows used) */
    Blast_ExtendWordExit(ewp, subject->length);

    Blast_UngappedStatsUpdate(ungapped_stats, totalhits, hits_extended,
                              ungapped_hsps->total);
    return 0;
}

/**
 * Beginning at s_off and q_off in the subject and query, respectively,
 * extend to the right until the cumulative score becomes negative or
 * drops by at least 'dropoff', or the end of at least one sequence 
 * is reached.
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_off subject offset [in]
 * @param q_off query offset [in]
 * @param dropoff the X dropoff parameter [in]
 * @param length the length of the computed extension [out]
 * @param maxscore the score derived from a previous left extension [in]
 * @param s_last_off the rightmost subject offset examined [out]
 * @return The score of the extension
 */
static Int4 s_BlastAaExtendRight(Int4 ** matrix,
                        const BLAST_SequenceBlk * subject,
                        const BLAST_SequenceBlk * query,
                        Int4 s_off, Int4 q_off, Int4 dropoff,
                        Int4 * length, Int4 maxscore, Int4 * s_last_off)
{
    Int4 i, n, best_i = -1;
    Int4 score = maxscore;

    Uint1 *s, *q;
    n = MIN(subject->length - s_off, query->length - q_off);

    s = subject->sequence + s_off;
    q = query->sequence + q_off;

    for (i = 0; i < n; i++) {
        score += matrix[q[i]][s[i]];

        if (score > maxscore) {
            maxscore = score;
            best_i = i;
        }

        /* The comparison below is really >= and is different than the old
           code (e.g., blast.c:BlastWordExtend_prelim). In the old code the
           loop continued as long as sum > X (X being negative).  The loop
           control here is different and we *break out* when the if statement 
           below is true. */
        if (score <= 0 || (maxscore - score) >= dropoff)
            break;
    }

    *length = best_i + 1;
    *s_last_off = s_off + i;
    return maxscore;
}


/**
 * Beginning at s_off and q_off in the subject and query, respectively,
 * extend to the left until the cumulative score drops by at least 
 * 'dropoff', or the end of at least one sequence is reached.
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_off subject offset [in]
 * @param q_off query offset [in]
 * @param dropoff the X dropoff parameter [in]
 * @param length the length of the computed extension [out]
 * @param maxscore the best running score from a previous extension;
 *                 the running score of the current extension must exceed 
 *                 this value if the extension is to be of nonzero size [in]
 * @return The score of the extension
 */
static Int4 s_BlastAaExtendLeft(Int4 ** matrix,
                       const BLAST_SequenceBlk * subject,
                       const BLAST_SequenceBlk * query,
                       Int4 s_off, Int4 q_off, Int4 dropoff, 
                       Int4 * length, Int4 maxscore)
{
    Int4 i, n, best_i;
    Int4 score = maxscore;

    Uint1 *s, *q;

    n = MIN(s_off, q_off);
    best_i = n + 1;

    s = subject->sequence + s_off - n;
    q = query->sequence + q_off - n;

    for (i = n; i >= 0; i--) {
        score += matrix[q[i]][s[i]];

        if (score > maxscore) {
            maxscore = score;
            best_i = i;
        }
        /* The comparison below is really >= and is different than the old
           code (e.g., blast.c:BlastWordExtend_prelim). In the old code the
           loop continued as long as sum > X (X being negative).  The loop
           control here is different and we *break out* when the if statement 
           below is true. */
        if ((maxscore - score) >= dropoff)
            break;
    }

    *length = n - best_i + 1;
    return maxscore;
}

/**
 * Identical to BlastAaExtendRight, except the score matrix
 * is position-specific
 *
 * @param matrix the substitution matrix representing query sequence [in]
 * @param subject subject sequence [in]
 * @param query_size number of rows in the scoring matrix [in]
 * @param s_off subject offset [in]
 * @param q_off query offset [in]
 * @param dropoff the X dropoff parameter [in]
 * @param length the length of the extension [out]
 * @param maxscore the score derived from a previous left extension [in]
 * @param s_last_off the rightmost subject offset examined [out]
 * @return The score of the extension
 */
static Int4 s_BlastPSSMExtendRight(Int4 ** matrix,
                          const BLAST_SequenceBlk * subject,
                          Int4 query_size, Int4 s_off, 
                          Int4 q_off, Int4 dropoff,
                          Int4 * length, Int4 maxscore, Int4 * s_last_off)
{
    Int4 i, n, best_i = -1;
    Int4 score = maxscore;
    Uint1 *s;

    n = MIN(subject->length - s_off, query_size - q_off);
    s = subject->sequence + s_off;

    for (i = 0; i < n; i++) {
        score += matrix[q_off + i][s[i]];

        if (score > maxscore) {
            maxscore = score;
            best_i = i;
        }

        /* The comparison below is really >= and is different than the old
           code (e.g., blast.c:BlastWordExtend_prelim). In the old code the
           loop continued as long as sum > X (X being negative).  The loop
           control here is different and we *break out* when the if statement 
           below is true. */
        if (score <= 0 || (maxscore - score) >= dropoff)
            break;
    }

    *length = best_i + 1;
    *s_last_off = s_off + i;
    return maxscore;
}

/**
 * Identical to BlastAaExtendLeft, except the query is 
 * represented by a position-specific score matrix.
 *
 * @param matrix the substitution matrix representing the query [in]
 * @param subject subject sequence [in]
 * @param s_off subject offset [in]
 * @param q_off query offset [in]
 * @param dropoff the X dropoff parameter [in]
 * @param length the length of the extension [out]
 * @param maxscore the score so far (probably from initial word hit) [in]
 * @return The score of the extension
 */
static Int4 s_BlastPSSMExtendLeft(Int4 ** matrix,
                         const BLAST_SequenceBlk * subject,
                         Int4 s_off, Int4 q_off,
                         Int4 dropoff, Int4 * length, Int4 maxscore)
{
    Int4 i, n, best_i;
    Int4 score = maxscore;
    Uint1 *s;

    n = MIN(s_off, q_off);
    best_i = n + 1;
    s = subject->sequence + s_off - n;

    for (i = n; i >= 0; i--) {
        score += matrix[q_off - n + i][s[i]];

        if (score > maxscore) {
            maxscore = score;
            best_i = i;
        }
        /* The comparison below is really >= and is different than the old
           code (e.g., blast.c:BlastWordExtend_prelim). In the old code the
           loop continued as long as sum > X (X being negative).  The loop
           control here is different and we *break out* when the if statement 
           below is true. */
        if ((maxscore - score) >= dropoff)
            break;
    }

    *length = n - best_i + 1;
    return maxscore;
}

static Int4 
s_BlastAaExtendOneHit(Int4 ** matrix,
                      const BLAST_SequenceBlk * subject,
                      const BLAST_SequenceBlk * query,
                      Int4 s_off, Int4 q_off,
                      Int4 dropoff, Int4 * hsp_q, Int4 * hsp_s,
                      Int4 * hsp_len, Int4 word_size, 
                      Boolean use_pssm, Int4 * s_last_off)
{
    Int4 score = 0, left_score, total_score, sum = 0;
    Int4 left_disp = 0, right_disp = 0;
    Int4 q_left_off = q_off, q_right_off =
        q_off + word_size, q_best_left_off = q_off;
    Int4 s_left_off, s_right_off;
    Int4 init_hit_width = 0;
    Int4 i;                     /* loop variable. */
    Uint1 *q = query->sequence;
    Uint1 *s = subject->sequence;

    for (i = 0; i < word_size; i++) {
        if (use_pssm)
            sum += matrix[q_off + i][s[s_off + i]];
        else
            sum += matrix[q[q_off + i]][s[s_off + i]];

        if (sum > score) {
            score = sum;
            q_best_left_off = q_left_off;
            q_right_off = q_off + i;
        } else if (sum <= 0) {
            sum = 0;
            q_left_off = q_off + i + 1;
        }
    }

    init_hit_width = q_right_off - q_left_off + 1;

    q_left_off = q_best_left_off;

    s_left_off = q_left_off + (s_off - q_off);
    s_right_off = q_right_off + (s_off - q_off);

    if (use_pssm) {
        left_score = s_BlastPSSMExtendLeft(matrix, subject,
                                           s_left_off - 1, q_left_off - 1,
                                           dropoff, &left_disp, score);

        total_score = s_BlastPSSMExtendRight(matrix, subject, query->length,
                                             s_right_off + 1, q_right_off + 1,
                                             dropoff, &right_disp, left_score,
                                             s_last_off);
    } else {
        left_score = s_BlastAaExtendLeft(matrix, subject, query,
                                         s_left_off - 1, q_left_off - 1,
                                         dropoff, &left_disp, score);

        total_score = s_BlastAaExtendRight(matrix, subject, query,
                                           s_right_off + 1, q_right_off + 1,
                                           dropoff, &right_disp, left_score,
                                           s_last_off);
    }

    *hsp_q = q_left_off - left_disp;
    *hsp_s = s_left_off - left_disp;
    *hsp_len = left_disp + right_disp + init_hit_width;

    return total_score;
}

static Int4
s_BlastAaExtendTwoHit(Int4 ** matrix,
                      const BLAST_SequenceBlk * subject,
                      const BLAST_SequenceBlk * query,
                      Int4 s_left_off, Int4 s_right_off, 
                      Int4 q_right_off, Int4 dropoff, 
                      Int4 * hsp_q, Int4 * hsp_s, Int4 * hsp_len,
                      Boolean use_pssm, Int4 word_size, 
                      Boolean * right_extend, Int4 * s_last_off)
{
    Int4 left_d = 0, right_d = 0;       /* left and right displacements */
    Int4 left_score = 0, right_score = 0;       /* left and right scores */
    Int4 i, score = 0;
    Uint1 *s = subject->sequence;
    Uint1 *q = query->sequence;

    /* find one beyond the position (up to word_size-1 letters to the right)
       that gives the largest starting score */
    /* Use "one beyond" to make the numbering consistent with how it's done
       for BlastAaExtendOneHit and the "Extend" functions called here. */
    for (i = 0; i < word_size; i++) {
        if (use_pssm)
            score += matrix[q_right_off + i][s[s_right_off + i]];
        else
            score += matrix[q[q_right_off + i]][s[s_right_off + i]];

        if (score > left_score) {
            left_score = score;
            right_d = i + 1;    /* Position is one beyond the end of the
                                   word. */
        }
    }
    q_right_off += right_d;
    s_right_off += right_d;

    right_d = 0;
    *right_extend = FALSE;
    *s_last_off = s_right_off;

    /* first, try to extend left, from the second hit to the first hit. */
    if (use_pssm)
        left_score = s_BlastPSSMExtendLeft(matrix, subject,
                                           s_right_off - 1, q_right_off - 1,
                                           dropoff, &left_d, 0);
    else
        left_score = s_BlastAaExtendLeft(matrix, subject, query,
                                         s_right_off - 1, q_right_off - 1,
                                         dropoff, &left_d, 0);

    /* Extend to the right only if left extension reached the first hit. */
    if (left_d >= (s_right_off - s_left_off)) {

        *right_extend = TRUE;
        if (use_pssm)
            right_score = s_BlastPSSMExtendRight(matrix, subject, query->length,
                                                 s_right_off, q_right_off,
                                                 dropoff, &right_d, left_score,
                                                 s_last_off);
        else
            right_score = s_BlastAaExtendRight(matrix, subject, query,
                                               s_right_off, q_right_off,
                                               dropoff, &right_d, left_score,
                                               s_last_off);
    }


    *hsp_q = q_right_off - left_d;
    *hsp_s = s_right_off - left_d;
    *hsp_len = left_d + right_d;
    return MAX(left_score, right_score);
}
