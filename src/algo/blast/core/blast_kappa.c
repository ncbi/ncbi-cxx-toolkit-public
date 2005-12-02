/* $Id$
 * ==========================================================================
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
 * Authors: Alejandro Schaffer, Mike Gertz (ported to algo/blast by Tom Madden)
 *
 */

/** @file blast_kappa.c
 * Utilities for doing Smith-Waterman alignments and adjusting the scoring
 * system for each match in blastpgp
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
"$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <float.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_kappa.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/link_hsps.h>
#include "blast_psi_priv.h"
#include "matrix_freq_ratios.h"
#include "blast_gapalign_priv.h"
#include "blast_posit.h"
#include "blast_hits_priv.h"

#include <algo/blast/composition_adjustment/nlm_linear_algebra.h>
#include <algo/blast/composition_adjustment/composition_constants.h>
#include <algo/blast/composition_adjustment/composition_adjustment.h>
#include <algo/blast/composition_adjustment/compo_heap.h>
#include <algo/blast/composition_adjustment/smith_waterman.h>
#include <algo/blast/composition_adjustment/redo_alignment.h>

/**
 * Scale the scores in an HSP list and reset the bit scores.
 *
 * @param hsp_list          the HSP list
 * @param logK              Karlin-Altschul statistical parameter [in]
 * @param lambda            Karlin-Altschul statistical parameter [in]
 * @param scoreDivisor      the value by which reported scores are to be
 * @todo rename to something which is more intention revealing, merge with
 * function of the same name in blast_traceback.c
 */
/* WHY */
static void
s_HSPListRescaleScores(BlastHSPList * hsp_list,
                       double lambda,
                       double logK,
                       double scoreDivisor)
{
    int hsp_index;
    for(hsp_index = 0; hsp_index < hsp_list->hspcnt; hsp_index++) {
        BlastHSP * hsp = hsp_list->hsp_array[hsp_index];

        hsp->score = BLAST_Nint(((double) hsp->score) / scoreDivisor);
        /* Compute the bit score using the newly computed scaled score. */
        hsp->bit_score = (hsp->score*lambda*scoreDivisor - logK)/NCBIMATH_LN2;
    }
}

/**
 * Remove from a hitlist all HSPs that are completely contained in an
 * HSP that occurs earlier in the list and that:
 * - is on the same strand; and
 * - has equal or greater score.  T
 * The hitlist should be sorted by some measure of significance before
 * this routine is called.
 * @param hsp_array         array to be reaped
 * @param hspcnt            length of hsp_array
 */
static void
s_HitlistReapContained(
                       BlastHSP * hsp_array[],
                       Int4 * hspcnt)
{
    Int4 iread;       /* iteration index used to read the hitlist */
    Int4 iwrite;      /* iteration index used to write to the hitlist */
    Int4 old_hspcnt;  /* number of HSPs in the hitlist on entry */

    old_hspcnt = *hspcnt;

    for (iread = 1;  iread < *hspcnt;  iread++) {
        /* for all HSPs in the hitlist */
        Int4         ireadBack;  /* iterator over indices less than iread */
        BlastHSP    *hsp1;       /* an HSP that is a candidate for deletion */

        hsp1 = hsp_array[iread];
        for (ireadBack = 0;  ireadBack < iread && hsp1 != NULL;  ireadBack++) {
            /* for all HSPs before hsp1 in the hitlist and while hsp1 has not
               been deleted */
            BlastHSP *hsp2;    /* an HSP that occurs earlier in hsp_array
                                * than hsp1 */
            hsp2 = hsp_array[ireadBack];

            if( hsp2 == NULL ) {  /* hsp2 was deleted in a prior iteration. */
                continue;
            }
            if (SIGN(hsp2->query.frame)   == SIGN(hsp1->query.frame) &&
                SIGN(hsp2->subject.frame) == SIGN(hsp1->subject.frame)) {
                /* hsp1 and hsp2 are in the same query/subject frame. */
                if (CONTAINED_IN_HSP
                    (hsp2->query.offset, hsp2->query.end, hsp1->query.offset,
                     hsp2->subject.offset, hsp2->subject.end,
                     hsp1->subject.offset) &&
                    CONTAINED_IN_HSP
                    (hsp2->query.offset, hsp2->query.end, hsp1->query.end,
                     hsp2->subject.offset, hsp2->subject.end,
                     hsp1->subject.end)    &&
                    hsp1->score <= hsp2->score) {
                    hsp1 = hsp_array[iread] = Blast_HSPFree(hsp_array[iread]);
                }
            } /* end if hsp1 and hsp2 are in the same query/subject frame */
        } /* end for all HSPs before hsp1 in the hitlist */
    } /* end for all HSPs in the hitlist */

    /* Condense the hsp_array, removing any NULL items. */
    iwrite = 0;
    for (iread = 0;  iread < *hspcnt;  iread++) {
        if (hsp_array[iread] != NULL) {
            hsp_array[iwrite++] = hsp_array[iread];
        }
    }
    *hspcnt = iwrite;
    /* Fill the remaining memory in hsp_array with NULL pointers. */
    for ( ;  iwrite < old_hspcnt;  iwrite++) {
        hsp_array[iwrite] = NULL;
    }
}


static void s_FreeEditScript(void * edit_script)
{
    if (edit_script != NULL)
        GapEditScriptDelete(edit_script);
}


/**
 * Converts a list of objects of type BlastCompo_Alignment to an
 * new object of type BlastHSPList and returns the result. Conversion
 * in this direction is lossless.  The list passed to this routine is
 * freed to ensure that there is no aliasing of fields between the
 * list of BlastCompo_Alignments and the new hitlist.
 *
 * @param alignments A list of distinct alignments; freed before return [in]
 * @param oid        Ordinal id of a database sequence [in]
 * @return Allocated and filled BlastHSPList strucutre.
 */
static BlastHSPList *
s_HSPListFromDistinctAlignments(
                                BlastCompo_Alignment ** alignments,
                                int oid)
{
    const int unknown_value = 0;
    BlastHSPList * hsp_list = Blast_HSPListNew(0);
    BlastCompo_Alignment * align;

    hsp_list->oid = oid;

    for (align = *alignments;  NULL != align;  align = align->next) {
        BlastHSP * new_hsp = NULL;
        GapEditScript * editScript = align->context;
        align->context = NULL;
        Blast_HSPInit(align->queryStart,   align->queryEnd,
                      align->matchStart,   align->matchEnd,
                      unknown_value, unknown_value,
                      0, 0, align->frame, align->score,
                      &editScript, &new_hsp);

        /* At this point, the subject and possibly the query sequence have
         * been filtered; since it is not clear that num_ident of the
         * filtered sequences, rather than the original, is desired,
         * explictly leave num_ident blank. */
        new_hsp->num_ident = 0;

        Blast_HSPListSaveHSP(hsp_list, new_hsp);
    }
    BlastCompo_AlignmentsFree(alignments, s_FreeEditScript);
    Blast_HSPListSortByScore(hsp_list);

    return hsp_list;
}


static void
s_HitlistEvaluateAndPurge(int * pbestScore, double *pbestEvalue,
                          BlastHSPList * hsp_list,
                          int subject_length,
                          EBlastProgramType program_number,
                          BlastQueryInfo* queryInfo,
                          BlastScoreBlk* sbp,
                          const BlastHitSavingParameters* hitParams,
                          int do_link_hsps)
{
    *pbestEvalue = DBL_MAX;
    *pbestScore  = 0;
    if (do_link_hsps) {
        BLAST_LinkHsps(program_number, hsp_list,
                       queryInfo, subject_length,
                       sbp, hitParams->link_hsp_params, TRUE);
    } else {
        Blast_HSPListGetEvalues(queryInfo, hsp_list, TRUE, sbp,
                                0.0, /* use a non-zero gap decay only when
                                        linking hsps */
                                1.0); /* Use scaling factor equal to
                                         1, because both scores and
                                         Lambda are scaled, so they
                                         will cancel each other. */
    }
    Blast_HSPListReapByEvalue(hsp_list, hitParams->options);
    if (hsp_list->hspcnt > 0) {
        *pbestEvalue = hsp_list->best_evalue;
        *pbestScore  = hsp_list->hsp_array[0]->score;
    }
}


static double
s_CalcLambda(double probs[], int min_score, int max_score, double lambda0)
{
    int i, n;
    double avg;
    Blast_ScoreFreq freq;

    n = max_score - min_score + 1;
    avg = 0.0;
    for (i = 0;  i < n;  i++) {
        avg += (min_score + i) * probs[i];
    }
    freq.score_min = min_score;
    freq.score_max = max_score;
    freq.obs_min = min_score;
    freq.obs_max = max_score;
    freq.sprob0 = probs;
    freq.sprob = &probs[-min_score];
    freq.score_avg = avg;

    return  Blast_KarlinLambdaNR(&freq, lambda0);
}


/** Return the a matrix of the frequency ratios that underlie the
 * score matrix being used on this pass. The returned matrix
 * is position-specific, so if we are in the first pass, use
 * query to convert the 20x20 standard matrix into a position-specific
 * variant. matrixName is the name of the underlying 20x20
 * score matrix used. numPositions is the length of the query;
 * startNumerator is the matrix of frequency ratios as stored
 * in posit.h. It needs to be divided by the frequency of the
 * second character to get the intended ratio
 * @param sbp statistical information for blast [in]
 * @param query the query sequence [in]
 * @param matrixName name of the underlying matrix [in]
 * @param startNumerator matrix of frequency ratios as stored
 *      in posit.h. It needs to be divided by the frequency of the
 *      second character to get the intended ratio [in]
 * @param numPositions length of the query [in]
 */
static void
s_GetStartFreqRatios(double ** returnRatios,
                     Uint1 * query,
                     const char *matrixName,
                     double **startNumerator,
                     Int4 numPositions,
                     Boolean positionSpecific)
{
    Int4 i,j;
    SFreqRatios * stdFreqRatios = NULL;
    const double kPosEpsilon = 0.0001;

    stdFreqRatios = _PSIMatrixFrequencyRatiosNew(matrixName);
    if (positionSpecific) {
        for (i = 0;  i < numPositions;  i++) {
            for (j = 0;  j < BLASTAA_SIZE;  j++) {
                returnRatios[i][j] = stdFreqRatios->data[query[i]][j];
            }
        }
    } else {
        for (i = 0;  i < BLASTAA_SIZE;  i++) {
            for (j = 0;  j < BLASTAA_SIZE;  j++) {
                returnRatios[i][j] = stdFreqRatios->data[i][j];
            }
        }
    }
    stdFreqRatios = _PSIMatrixFrequencyRatiosFree(stdFreqRatios);

    if (positionSpecific) {
        double *standardProb; /*probabilities of each letter*/
        standardProb = BLAST_GetStandardAaProbabilities();

        /*reverse multiplication done in posit.c*/
        for (i = 0;  i < numPositions;  i++) {
            for (j = 0;  j < BLASTAA_SIZE;  j++) {
                if ((standardProb[query[i]] > kPosEpsilon) &&
                    (standardProb[j] > kPosEpsilon) &&
                    (j != eStopChar) && (j != eXchar) &&
                    (startNumerator[i][j] > kPosEpsilon)) {
                    returnRatios[i][j] = startNumerator[i][j]/standardProb[j];
                }
            }
        }
        sfree(standardProb);
    }
}


/** SCALING_FACTOR is a multiplicative factor used to get more bits of
 * precision in the integer matrix scores. It cannot be arbitrarily
 * large because we do not want total alignment scores to exceedto
 * -(BLAST_SCORE_MIN) */
#define SCALING_FACTOR 32


/**
 * produce a scaled-up version of the position-specific matrix
 * starting from posFreqs
 *
 * @param fillPosMatrix     is the matrix to be filled
 * @param nonposMatrix      is the underlying position-independent matrix,
 *                          used to fill positions where frequencies are
 *                          irrelevant
 * @param sbp               stores various parameters of the search
 * @param matrixName        name of the standard substitution matrix [in]
 * @param posFreqs          PSSM's frequency ratios [in]
 * @param query             Query sequence data [in]
 * @param queryLength       Length of the query sequence above [in]
 */
static int
s_ScalePosMatrix(int **fillPosMatrix,
                 int **nonposMatrix,
                 const char *matrixName,
                 double **posFreqs,
                 Uint1 *query,
                 int queryLength,
                 BlastScoreBlk* sbp)
{
    Kappa_posSearchItems *posSearch = NULL;
    Kappa_compactSearchItems *compactSearch = NULL;
    _PSIInternalPssmData* internal_pssm = NULL;
    int status = PSI_SUCCESS;

    posSearch = Kappa_posSearchItemsNew(queryLength, matrixName,
                                        fillPosMatrix, posFreqs);
    compactSearch = Kappa_compactSearchItemsNew(query, queryLength, sbp);

    /* Copy data into new structures */
    internal_pssm = _PSIInternalPssmDataNew(queryLength, BLASTAA_SIZE);
    _PSICopyMatrix_int(internal_pssm->pssm, posSearch->posMatrix,
                       internal_pssm->ncols, internal_pssm->nrows);
    _PSICopyMatrix_int(internal_pssm->scaled_pssm,
                       posSearch->posPrivateMatrix,
                       internal_pssm->ncols, internal_pssm->nrows);
    _PSICopyMatrix_double(internal_pssm->freq_ratios,
                          posSearch->posFreqs, internal_pssm->ncols,
                          internal_pssm->nrows);
    status = _PSIConvertFreqRatiosToPSSM(internal_pssm, query, sbp,
                                         compactSearch->standardProb);
    if (status != PSI_SUCCESS) {
        internal_pssm = _PSIInternalPssmDataFree(internal_pssm);
        posSearch = Kappa_posSearchItemsFree(posSearch);
        compactSearch = Kappa_compactSearchItemsFree(compactSearch);
        return status;
    }
    /* Copy data from new structures to posSearchItems */
    _PSICopyMatrix_int(posSearch->posMatrix, internal_pssm->pssm,
                       internal_pssm->ncols, internal_pssm->nrows);
    _PSICopyMatrix_int(posSearch->posPrivateMatrix,
                       internal_pssm->scaled_pssm,
                       internal_pssm->ncols, internal_pssm->nrows);
    _PSICopyMatrix_double(posSearch->posFreqs,
                          internal_pssm->freq_ratios,
                          internal_pssm->ncols, internal_pssm->nrows);
    status = Kappa_impalaScaling(posSearch, compactSearch, (double)
                                 SCALING_FACTOR, FALSE, sbp);
    if (status != 0) {
        internal_pssm = _PSIInternalPssmDataFree(internal_pssm);
        posSearch = Kappa_posSearchItemsFree(posSearch);
        compactSearch = Kappa_compactSearchItemsFree(compactSearch);
        return status;
    }
    internal_pssm = _PSIInternalPssmDataFree(internal_pssm);
    posSearch = Kappa_posSearchItemsFree(posSearch);
    compactSearch = Kappa_compactSearchItemsFree(compactSearch);
    return status;
}


static BlastCompo_Alignment *
s_ResultHspToDistinctAlign(BlastQueryInfo* queryInfo,
                           BlastHSP * hsp_array[], Int4 hspcnt,
                           double localScalingFactor)
{
    BlastCompo_Alignment *aligns = NULL, *tail = NULL, *new_align = NULL;
    int hsp_index;
    for (hsp_index = 0;  hsp_index < hspcnt;  hsp_index++) {
        int queryIndex, queryEnd, matchEnd;
        BlastHSP * hsp = hsp_array[hsp_index];
        queryEnd = hsp->query.end;
        matchEnd = hsp->subject.end;
        /* YIKES! how do we handle multiple queries */
        /*
          if(search->mult_queries != NULL) {
          queryIndex =
          GetQueryNum(search->mult_queries,
          hsp->query_offset, queryEnd - 1, 0);
          } else {
          queryIndex = 0;
          }
        */
        queryIndex = 0;
        new_align =
            BlastCompo_AlignmentNew(hsp->score * localScalingFactor,
                                    eNoCompositionAdjustment,
                                    hsp->query.offset, queryEnd, queryIndex,
                                    hsp->subject.offset, matchEnd,
                                    hsp->subject.frame, hsp);
        if (new_align == NULL) /* out of memory */
            goto error_return;
        if (tail == NULL) {
            aligns = new_align;
        } else {
            tail->next = new_align;
        }
        tail = new_align;
    }
    goto normal_return;
 error_return:
    BlastCompo_AlignmentsFree(&aligns, NULL);
 normal_return:
    return aligns;
}


/**
 * Redo a S-W alignment using an x-drop alignment.  The result will
 * usually be the same as the S-W alignment. The call to ALIGN
 * attempts to force the endpoints of the alignment to match the
 * optimal endpoints determined by the Smith-Waterman algorithm.
 * ALIGN is used, so that if the data structures for storing BLAST
 * alignments are changed, the code will not break
 *
 * @param query         the query data
 * @param queryStart    start of the alignment in the query sequence
 * @param queryEnd      end of the alignment in the query sequence,
 *                      as computed by the Smith-Waterman algorithm
 * @param subject       the subject (database) sequence
 * @param matchStart    start of the alignment in the subject sequence
 * @param matchEnd      end of the alignment in the query sequence,
 *                      as computed by the Smith-Waterman algorithm
 * @param scoringParams Settings for gapped alignment.[in]
 * @param gap_align     parameters for a gapped alignment
 * @param score         score computed by the Smith-Waterman algorithm
 * @param localScalingFactor    the factor by which the scoring system has
 *                              been scaled in order to obtain greater
 *                              precision
 * @param queryAlignmentExtent  length of the alignment in the query sequence,
 *                              as computed by the x-drop algorithm
 * @param matchAlignmentExtent  length of the alignment in the subject
 *                              sequence, as computed by the x-drop algorithm
 * @param newScore              alignment score computed by the x-drop
 *                              algorithm
 */
static void
s_SWFindFinalEndsUsingXdrop(
                            BlastCompo_SequenceData * query,
                            Int4 queryStart,
                            Int4 queryEnd,
                            BlastCompo_SequenceData * subject,
                            Int4 matchStart,
                            Int4 matchEnd,
                            BlastGapAlignStruct* gap_align,
                            const BlastScoringParameters* scoringParams,
                            Int4 score,
                            double localScalingFactor,
                            Int4 * queryAlignmentExtent,
                            Int4 * matchAlignmentExtent,
                            Int4 * newScore)
{
    Int4 XdropAlignScore;         /* alignment score obtained using X-dropoff
                                   * method rather than Smith-Waterman */
    Int4 doublingCount = 0;       /* number of times X-dropoff had to be
                                   * doubled */

    GapPrelimEditBlockReset(gap_align->rev_prelim_tback);
    GapPrelimEditBlockReset(gap_align->fwd_prelim_tback);
    do {
        XdropAlignScore =
            ALIGN_EX(&(query->data[queryStart]) - 1,
                     &(subject->data[matchStart]) - 1,
                     queryEnd - queryStart + 1, matchEnd - matchStart + 1,
                     queryAlignmentExtent,
                     matchAlignmentExtent, gap_align->fwd_prelim_tback,
                     gap_align, scoringParams, queryStart - 1, FALSE, FALSE);

        gap_align->gap_x_dropoff *= 2;
        doublingCount++;
        if((XdropAlignScore < score) && (doublingCount < 3)) {
            GapPrelimEditBlockReset(gap_align->fwd_prelim_tback);
        }
    } while((XdropAlignScore < score) && (doublingCount < 3));

    *newScore = XdropAlignScore;
}


/**
 * A Kappa_MatchingSequence represents a subject sequence to be aligned
 * with the query.  This abstract sequence is used to hide the
 * complexity associated with actually obtaining and releasing the
 * data for a matching sequence, e.g. reading the sequence from a DB
 * or translating it from a nucleotide sequence.
 *
 * We draw a distinction between a sequence itself, and strings of
 * data that may be obtained from the sequence.  The amino
 * acid/nucleotide data is represented by an object of type
 * BlastCompo_SequenceData.  There may be more than one instance of
 * BlastCompo_SequenceData per Kappa_MatchingSequence, each representing a
 * different range in the sequence, or a different translation frame.
 */
typedef struct Kappa_SequenceLocalData {
    EBlastProgramType prog_number; /**< identifies the type of blast
                                        search being performed. The type
                                        of search determines how sequence
                                        data should be obtained. */
    const Uint1*   genetic_code;   /**< genetic code for translated searches */
    const BlastSeqSrc* seq_src;    /**< BLAST sequence data source */
    BlastSeqSrcGetSeqArg seq_arg;  /**< argument to GetSequence method
                                     of the BlastSeqSrc (@todo this
                                     structure was designed to be
                                     allocated on the stack, i.e.: in
                                     Kappa_MatchingSequenceInitialize) */
} Kappa_SequenceLocalData;


/**
 * Initialize a new matching sequence, obtaining information about the
 * sequence from the search.
 *
 * @param self              object to be initialized
 * @param seqSrc            A pointer to a source from which sequence data
 *                          may be obtained
 * @param program_number    identifies the type of blast search being
 *                          performed.
 * @param gen_code_string   genetic code for translated queries
 * @param subject_index     index of the matching sequence in the database
 */
static void
s_MatchingSequenceInitialize(
                             BlastCompo_MatchingSequence * self,
                             EBlastProgramType program_number,
                             const BlastSeqSrc* seqSrc,
                             const Uint1* gen_code_string,
                             Int4 subject_index)
{
    Kappa_SequenceLocalData * local_data =
        malloc(sizeof(Kappa_SequenceLocalData));
    self->local_data = local_data;

    local_data->seq_src      = seqSrc;
    local_data->prog_number  = program_number;
    local_data->genetic_code = gen_code_string;

    memset((void*) &local_data->seq_arg, 0, sizeof(local_data ->seq_arg));
    local_data->seq_arg.oid = self->index = subject_index;

    if( program_number == eBlastTypeTblastn ) {
        local_data->seq_arg.encoding = eBlastEncodingNcbi4na;
    } else {
        local_data->seq_arg.encoding = eBlastEncodingProtein;
    }
    if (BlastSeqSrcGetSequence(seqSrc, (void*) &local_data->seq_arg) >= 0) {
        self->length =
            BlastSeqSrcGetSeqLen(seqSrc, (void*) &local_data->seq_arg);
    } else {
        self->length = 0;
    }
}


/** Release the resources associated with a matching sequence. */
static void
s_MatchingSequenceRelease(BlastCompo_MatchingSequence * self)
{
    if (self != NULL) {
        Kappa_SequenceLocalData * local_data = self->local_data;
        BlastSeqSrcReleaseSequence(local_data->seq_src,
                                   (void*)&local_data->seq_arg);
        BlastSequenceBlkFree(local_data->seq_arg.seq);
        free(self->local_data);
        self->local_data = NULL;
    }
}


/** NCBIstdaa encoding for 'X' character */
#define BLASTP_MASK_RESIDUE 21
/** Default instructions and mask residue for SEG filtering */
#define BLASTP_MASK_INSTRUCTIONS "S 10 1.8 2.1"


/**
 * Obtain a string of translated data
 *
 * @param self          the sequence from which to obtain the data [in]
 * @param range         the range and translation frame to get [in]
 * @param seqData       the resulting data [out]
 */
static void
s_SequenceGetTranslatedRange(const BlastCompo_MatchingSequence * self,
                             const BlastCompo_SequenceRange * range,
                             BlastCompo_SequenceData * seqData )
{
    ASSERT( 0 && "Not implemented" );
}


/**
 * Obtain the sequence data that lies within the given range.
 *
 * @param self          sequence information [in]
 * @param range        range specifying the range of data [in]
 * @param seqData       the sequence data obtained [out]
 */
static int
s_SequenceGetRange(
                   const BlastCompo_MatchingSequence * self,
                   const BlastCompo_SequenceRange * range,
                   BlastCompo_SequenceData * seqData )
{
    Kappa_SequenceLocalData * local_data = self->local_data;
    if (local_data->prog_number ==  eBlastTypeTblastn) {
        /* The sequence must be translated. */
        s_SequenceGetTranslatedRange(self, range, seqData);
    } else {
        /* The sequence does not need to be translated. */
        Int4       idx;
        Uint1     *origData;  /* the unfiltered data for the sequence */

        /* Copy the entire sequence (necessary for SEG filtering.) */
        seqData->buffer  = calloc((self->length + 2), sizeof(Uint1));
        /* First and last characters of the buffer MUST be '\0', which is
         * true here because the buffer was allocated using calloc. */
        seqData->data    = seqData->buffer + 1;
        seqData->length  = self->length;

        origData = local_data->seq_arg.seq->sequence;
        for (idx = 0;  idx < seqData->length;  idx++) {
            /* Copy the sequence data, replacing occurrences of amino acid
             * number 24 (Selenocysteine) with number 21 (Undetermined or
             * atypical). */
            if (origData[idx] != 24) {
                seqData->data[idx] = origData[idx];
            } else {
                seqData->data[idx] = 21;
                fprintf(stderr, "Selenocysteine (U) at position %ld"
                        " replaced by X\n",
                        (long) idx + 1);
            }
        }
#ifndef KAPPA_NO_SEG_SEQUENCE
        /* take as input an amino acid  string and its length; compute
         * a filtered amino acid string and return the filtered string */
        {{
            BlastSeqLoc* mask_seqloc;
            const EBlastProgramType k_program_name = eBlastTypeBlastp;
            SBlastFilterOptions* filter_options;

            BlastFilteringOptionsFromString(k_program_name,
                                            BLASTP_MASK_INSTRUCTIONS,
                                            &filter_options, NULL);
            BlastSetUp_Filter(k_program_name, seqData->data, seqData->length,
                              0, filter_options, &mask_seqloc, NULL);
            filter_options = SBlastFilterOptionsFree(filter_options);

            Blast_MaskTheResidues(seqData->data, seqData->length,
                                  FALSE, mask_seqloc, FALSE, 0);
            mask_seqloc = BlastSeqLocFree(mask_seqloc);
        }}
#endif
        /* Fit the data to the range. */
        seqData ->data    = &seqData->data[range->begin - 1];
        *seqData->data++  = '\0';
        seqData ->length  = range->end - range->begin;
    } /* end else the sequence does not need to be translated */
    return 0;
}


/**
 * Computes an appropriate starting point for computing the traceback
 * for an HSP.  The start point depends on the matrix, the window, and
 * the filtered sequence, and so may not be the start point saved in
 * the HSP during the preliminary gapped extension.
 *
 * @param q_start       the start point in the query [out]
 * @param s_start       the start point in the subject [out]
 * @param sbp           general scoring info (includes the matrix) [in]
 * @param positionBased is this search position-specific? [in]
 * @param hsp           the HSP to be considered [in]
 * @param window        the window used to compute the traceback [in]
 * @param query         the query data [in]
 * @param subject       the subject data [in]
 */
/* WHY */
static void
s_StartingPointForHit(Int4 * q_start,
                      Int4 * s_start,
                      const BlastScoreBlk* sbp,
                      Boolean positionBased,
                      BlastHSP * hsp,
                      BlastCompo_SequenceRange * range,
                      BlastCompo_SequenceData * query,
                      BlastCompo_SequenceData * subject)
{
    hsp->subject.offset       -= range->begin;
    hsp->subject.gapped_start -= range->begin;

    if(BLAST_CheckStartForGappedAlignment(hsp, query->data,
                                          subject->data, sbp)) {
        /* We may use the starting point supplied by the HSP. */
        *q_start = hsp->query.gapped_start;
        *s_start = hsp->subject.gapped_start;
    } else {
        /* We must recompute the start for the gapped alignment, as the
           one in the HSP was unacceptable.*/
        *q_start =
            BlastGetStartForGappedAlignment(query->data,
                                            subject->data, sbp,
                                            hsp->query.offset,
                                            hsp->query.end -
                                            hsp->query.offset,
                                            hsp->subject.offset,
                                            hsp->subject.end -
                                            hsp->subject.offset);
        *s_start =
            (hsp->subject.offset - hsp->query.offset) + *q_start;
    }
}


struct Blast_GappingParamsContext {
    BlastGapAlignStruct * gap_align;
    const BlastScoringParameters* scoringParams;
    BlastScoreBlk* sbp;
    double localScalingFactor;
    Int4 prog_number;
};
typedef struct Blast_GappingParamsContext Blast_GappingParamsContext;


/**
 * Reads a GapAlignBlk that has been used to compute a traceback, and
 * return a BlastCompo_Alignment representing the alignment.
 *
 * @param gap_align         the GapAlignBlk
 * @param window            the window used to compute the traceback
 */
static BlastCompo_Alignment *
s_NewAlignmentFromGapAlign(BlastGapAlignStruct * gap_align,
                           BlastCompo_SequenceRange * query_range,
                           BlastCompo_SequenceRange * subject_range,
                           int whichMode)
{
    int queryStart, queryEnd, queryIndex, matchStart, matchEnd, frame;
    BlastCompo_Alignment * obj; /* the new alignment */

    queryStart = gap_align->query_start + query_range->begin;
    queryEnd   = gap_align->query_stop + query_range->begin;
    queryIndex = query_range->context;
    matchStart = gap_align->subject_start + subject_range->begin;
    matchEnd   = gap_align->subject_stop  + subject_range->begin;
    frame      = subject_range->context;

    obj = BlastCompo_AlignmentNew(gap_align->score, whichMode,
                                  queryStart, queryEnd, queryIndex,
                                  matchStart, matchEnd, frame,
                                  gap_align->edit_script);
    gap_align->edit_script = NULL;
    return obj;
}


/**
 * Create a new BlastCompo_Alignment and append the list of
 * alignments represented by "next."
 *
 * @param query         query sequence data
 * @param queryStart    the start of the alignment in the query
 * @param queryEnd      the end of the alignment in the query
 * @param subject       subject sequence data
 * @param matchStart    the start of the alignment in the subject range
 * @param matchEnd      the end of the alignment in the subject range
 * @param score         the score of this alignment
 * @param window        the subject window of this alignment
 * @param gap_align     alignment info for gapped alignments
 * @param scoringParams Settings for gapped alignment.[in]
 * @param localScalingFactor    the factor by which the scoring system has
 *                              been scaled in order to obtain greater
 *                              precision
 * @param prog_number   the type of alignment being performed
 * @param next          preexisting list of alignments [out]
 */
static int
s_NewAlignmentUsingXdrop(BlastCompo_Alignment ** pnewAlign,
                         Int4 * pqueryEnd, Int4 *pmatchEnd,
                         Int4 queryStart, Int4 matchStart, Int4 score,
                         BlastCompo_SequenceData * query,
                         BlastCompo_SequenceRange * query_range,
                         Int4 queryLength,
                         BlastCompo_SequenceData * subject,
                         BlastCompo_SequenceRange * subject_range,
                         Int4 subjectLength,
                         BlastCompo_GappingParams * gapping_params,
                         ECompoAdjustModes whichMode)
{
    Int4 newScore;
    /* Extent of the alignment as computed by an x-drop alignment
     * (usually the same as (queryEnd - queryStart) and (matchEnd -
     * matchStart)) */
    Int4 queryExtent, matchExtent;
    BlastCompo_Alignment * obj;  /* the new object */
    Blast_GappingParamsContext * context = gapping_params->context;
    BlastGapAlignStruct * gap_align = context->gap_align;
    const BlastScoringParameters* scoringParams = context->scoringParams;
    double localScalingFactor = context->localScalingFactor;
    GapEditScript* editScript;

    s_SWFindFinalEndsUsingXdrop(query,   queryStart, *pqueryEnd,
                                subject, matchStart, *pmatchEnd,
                                gap_align, scoringParams,
                                score, localScalingFactor,
                                &queryExtent, &matchExtent,
                                &newScore);
    *pqueryEnd = queryStart + queryExtent;
    *pmatchEnd = matchStart + matchExtent;

    editScript =
        Blast_PrelimEditBlockToGapEditScript(gap_align->rev_prelim_tback,
                                             gap_align->fwd_prelim_tback);
    obj = BlastCompo_AlignmentNew(newScore, whichMode,
                                  queryStart, *pqueryEnd,
                                  query_range->context,
                                  matchStart, *pmatchEnd,
                                  subject_range->context, editScript);
    *pnewAlign = obj;

    return 0;
}


static BlastCompo_Alignment *
s_RedoOneAlignment(BlastCompo_Alignment * in_align,
                   ECompoAdjustModes whichMode,
                   BlastCompo_SequenceData * query_data,
                   BlastCompo_SequenceRange * query_range,
                   int ccat_query_length,
                   BlastCompo_SequenceData * subject_data,
                   BlastCompo_SequenceRange * subject_range,
                   int full_subject_length,
                   BlastCompo_GappingParams * gapping_params)
{
    Int4 q_start, s_start;
    Blast_GappingParamsContext * context = gapping_params->context;
    BlastScoreBlk* sbp = context->sbp;
    BlastGapAlignStruct* gapAlign = context->gap_align;
    Boolean positionBased = (sbp->psi_matrix ? TRUE : FALSE);
    BlastHSP * hsp = in_align->context;

    s_StartingPointForHit(&q_start, &s_start, sbp, positionBased,
                          hsp, subject_range, query_data, subject_data);
    gapAlign->gap_x_dropoff = gapping_params->x_dropoff;

    BLAST_GappedAlignmentWithTraceback(context->prog_number,
                                       query_data->data,
                                       subject_data->data, gapAlign,
                                       context->scoringParams,
                                       q_start, s_start,
                                       query_data->length,
                                       subject_data->length);
    return s_NewAlignmentFromGapAlign(gapAlign, query_range, subject_range,
                                      whichMode);
}


/**
 * A s_SearchParameters represents the data needed by
 * RedoAlignmentCore to adjust the parameters of a search, including
 * the original value of these parameters
 */
typedef struct s_SearchParameters {
    Int4          gap_open;    /**< a penalty for the existence of a gap */
    Int4          gapExtend;   /**< a penalty for each residue in the
                                    gap */
    Int4          gapDecline;  /**< a penalty for declining to align a pair
                                    of residues */
    double        scale_factor;     /**< the original scale factor */
    Int4 **origMatrix;              /**< The original matrix values */
    double original_expect_value;   /**< expect value on entry */
    /** copy of the original gapped Karlin-Altschul block
     * corresponding to the first context */
    Blast_KarlinBlk* kbp_gap_orig;
    /** pointer to the array of gapped Karlin-Altschul block for all
     * contexts; needed to restore the search to its original
     * configuration.  */
    Blast_KarlinBlk** orig_kbp_gap_array;
} s_SearchParameters;


/**
 * Release the data associated with a s_SearchParameters and
 * delete the object
 * @param searchParams the object to be deleted [in][out]
 */
static void
s_SearchParametersFree(s_SearchParameters ** searchParams)
{
    /* for convenience, remove one level of indirection from searchParams */
    s_SearchParameters *sp = *searchParams;

    if(sp->kbp_gap_orig) Blast_KarlinBlkFree(sp->kbp_gap_orig);

    Nlm_Int4MatrixFree(&sp->origMatrix);

    sfree(*searchParams);
    *searchParams = NULL;
}


/**
 * Create a new instance of s_SearchParameters
 *
 * @param rows              number of rows in the scoring matrix
 * @param adjustParameters  if >0, use composition-based statistics
 * @param numQueries        the number of queries in the concatenated
 *                          query
 * @param positionBased     if true, the search is position-based
 */
static s_SearchParameters *
s_SearchParametersNew(
                      Int4 rows,
                      Int4 adjustParameters,
                      Boolean positionBased)
{
    s_SearchParameters *sp;   /* the new object */
    sp = malloc(sizeof(s_SearchParameters));

    sp->orig_kbp_gap_array = NULL;
    sp->kbp_gap_orig       = NULL;
    sp->origMatrix         = NULL;

    sp->kbp_gap_orig = Blast_KarlinBlkNew();
    if (adjustParameters) {
        if (positionBased) {
            sp->origMatrix = Nlm_Int4MatrixNew(rows, BLASTAA_SIZE);
        } else {
            sp->origMatrix = Nlm_Int4MatrixNew(BLASTAA_SIZE, BLASTAA_SIZE);
        }
    }
    return sp;
}


/**
 * Record the initial value of the search parameters that are to be
 * adjusted.
 *
 * @param searchParams     holds the recorded values [out]
 * @param search           the search parameters [in]
 * @param query            a list of query data [in]
 * @param numQueries       the length of the array query [in]
 */
static void
s_RecordInitialSearch(s_SearchParameters * searchParams,
                      BLAST_SequenceBlk * queryBlk,
                      BlastQueryInfo* queryInfo,
                      BlastScoreBlk* sbp,
                      const BlastScoringParameters* scoring,
                      int query_length,
                      Boolean adjustParameters,
                      Boolean positionBased)
{
    Blast_KarlinBlk* kbp;     /* statistical parameters used to evaluate a
                               * query-subject pair */
    /* YIKES! How do I get these! */
    /*
      searchParams->original_expect_value = search->pbp->cutoff_e;
    */
    searchParams->gap_open   = scoring->gap_open;
    searchParams->gapExtend  = scoring->gap_extend;
    searchParams->gapDecline = scoring->decline_align;
    searchParams->scale_factor = scoring->scale_factor;

    searchParams->orig_kbp_gap_array   = sbp->kbp_gap;
    kbp = sbp->kbp_gap[0];
    Blast_KarlinBlkCopy(searchParams->kbp_gap_orig, kbp);

    if (adjustParameters) {
        Int4 **matrix;
        Int4 i, j;                  /* iteration indices */
        int rows;
        if (positionBased) {
            matrix = sbp->psi_matrix->pssm->data;
            rows = query_length;
        } else {
            matrix = sbp->matrix->data;
            rows = BLASTAA_SIZE;
        }
        for (i = 0;  i < rows;  i++) {
            for (j = 0;  j < BLASTAA_SIZE;  j++) {
                searchParams->origMatrix[i][j] = matrix[i][j];
            }
        }
    }
}


/**
 * Rescale the search parameters in the search object and options
 * object to obtain more precision.
 */
static void
s_RescaleSearch(s_SearchParameters * sp,
                BLAST_SequenceBlk* queryBlk,
                BlastQueryInfo* queryInfo,
                BlastScoreBlk* sbp,
                BlastScoringParameters* scoringParams,
                double localScalingFactor,
                Boolean positionBased)
{
    Blast_KarlinBlk* kbp;     /* the statistical parameters used to
                               * evaluate alignments of a
                               * query-subject pair */
    kbp = sbp->kbp_gap[0];
    kbp->Lambda /= localScalingFactor;
    kbp->logK = log(kbp->K);

    /* YIKES! and what about the cutoff_e */
    /*
      search->pbp->cutoff_e   = options->kappa_expect_value;
    */
    scoringParams->gap_open   = BLAST_Nint(sp->gap_open  * localScalingFactor);
    scoringParams->gap_extend = BLAST_Nint(sp->gapExtend * localScalingFactor);
    scoringParams->scale_factor = localScalingFactor;
    if (sp->gapDecline != INT2_MAX) {
        scoringParams->decline_align =
            BLAST_Nint(sp->gapDecline * localScalingFactor);
    }
}


/**
 * Restore the parameters that were adjusted to their original values
 * @param searchParams      a record of the original values [in]
 * @param search            the search to be restored [out]
 * @param options           the option block to be restored [out]
 * @param matrix            the scoring matrix to be restored [out]
 * @param SmithWaterman     if true, we have performed a Smith-Waterman
 *                          alignment with these search parameters [in]
 */
static void
s_RestoreSearch(s_SearchParameters * searchParams,
                BlastScoreBlk* sbp,
                Int4 ** matrix,
                int query_length,
                BlastScoringParameters* scoring,
                Boolean positionBased,
                Boolean adjustParameters)
{
    Blast_KarlinBlk* kbp;       /* statistical parameters used to
                                   evaluate the significance of
                                   alignment of a query-subject
                                   pair */
    Int4 i, j;
    /* YIKES! More stuff I don't know how to deal with */
    /*
      search->pbp->gap_x_dropoff_final = searchParams->gap_x_dropoff_final;
      search->pbp->cutoff_e      = searchParams->original_expect_value;
      search->pbp->gap_open      = searchParams->gap_open;
      search->pbp->gap_extend    = searchParams->gapExtend;
      search->pbp->decline_align = searchParams->gapDecline;
      GapAlignBlkDelete(search->gap_align);
      search->gap_align    = searchParams->orig_gap_align;
      search->sbp->kbp_gap = searchParams->orig_kbp_gap_array;
    */
    kbp = sbp->kbp_gap[0];
    Blast_KarlinBlkCopy(kbp, searchParams->kbp_gap_orig);

    if(adjustParameters) {
        int rows;
        if (positionBased) {
            rows = query_length;
        } else {
            rows = BLASTAA_SIZE;
        }
        for(i = 0; i < rows; i++) {
            for(j = 0; j < BLASTAA_SIZE; j++) {
                matrix[i][j] = searchParams->origMatrix[i][j];
            }
        }
    }
}


static void
s_MatrixInfoInit(Blast_MatrixInfo * self,
                 double localScalingFactor,
                 BLAST_SequenceBlk* queryBlk,
                 BlastQueryInfo* queryInfo,
                 BlastScoreBlk* sbp,
                 BlastScoringParameters* scoringParams,
                 Boolean positionBased,
                 const char * matrixName)
{
    Uint1 * query;             /* the query sequence */
    int queryLength;
    /*  Int4 queryLength; */          /* the length of the query sequence */
    double initialUngappedLambda;

    /* YIKES! */
    /*
      query       = search->context[0].query->sequence;
      queryLength = search->context[0].query->length;
    */
    query = &queryBlk->sequence[0];
    queryLength = queryInfo->contexts[0].query_length;
    if (self->positionBased) {
        /*  YIKES!
            if(sbp->posFreqs == NULL) {
            sbp->posFreqs =
            allocatePosFreqs(queryLength, BLASTAA_SIZE);
            }
        */
        s_GetStartFreqRatios(self->startFreqRatios, query, matrixName,
                             sbp->psi_matrix->freq_ratios, queryLength,
                             TRUE);
        s_ScalePosMatrix(self->startMatrix, sbp->matrix->data,
                         matrixName,sbp->psi_matrix->freq_ratios, query,
                         queryInfo->max_length, sbp);
        initialUngappedLambda = sbp->kbp_psi[0]->Lambda;
    } else {
        s_GetStartFreqRatios(self->startFreqRatios, query, matrixName,
                             NULL, BLASTAA_SIZE, FALSE);
        initialUngappedLambda = sbp->kbp_ideal->Lambda;
    }
    self->ungappedLambda = initialUngappedLambda / localScalingFactor;
    if ( !positionBased ) {
        SFreqRatios * freqRatios;  /* frequency ratios for the matrix */

        freqRatios = _PSIMatrixFrequencyRatiosNew(matrixName);
        /*
          if (freqRatios == NULL) {
          ErrPostEx(SEV_FATAL, 1, 0, "blastpgp: Cannot adjust parameters "
          "for matrix %s\n", matrixName);
          }
        */
        Blast_Int4MatrixFromFreq(self->startMatrix, BLASTAA_SIZE,
                                 freqRatios->data, self->ungappedLambda);
        freqRatios = _PSIMatrixFrequencyRatiosFree(freqRatios);
    }
    self->matrixName = strdup(matrixName);
}


static void
s_GetQueryInfo(BlastCompo_QueryInfo **pquery, int * pnumQueries,
               Uint1 * ccat_query, BlastQueryInfo* queryInfo)
{
    int query_index;
    int numQueries = queryInfo->num_queries;
    BlastCompo_QueryInfo * query = calloc(numQueries,
                                          sizeof(BlastCompo_QueryInfo));
    *pnumQueries = numQueries;
    *pquery = query;
    for (query_index = 0;  query_index < numQueries;  query_index++) {
        query[query_index].eff_search_space =
            queryInfo->contexts[query_index].eff_searchsp;
    }
    for (query_index = 0;  query_index < numQueries;  query_index++) {
        query[query_index].origin =
            queryInfo->contexts[query_index].query_offset;
        query[query_index].seq.data = &ccat_query[query[query_index].origin];
        query[query_index].seq.length =
            queryInfo->contexts[query_index].query_length;
    }
    for (query_index = 0;  query_index < numQueries;  query_index++) {
        Blast_ReadAaComposition(&query[query_index].composition,
                                query[query_index].seq.data,
                                query[query_index].seq.length);
    }
}


static void
s_GappingParamsInit(Blast_GappingParamsContext * context,
                    BlastCompo_GappingParams * gapping_params,
                    BlastGapAlignStruct * gap_align,
                    const BlastScoringParameters* scoring,
                    BlastScoreBlk* sbp,
                    double localScalingFactor,
                    Int4 program_number,
                    double Lambda)
{
    context->gap_align = gap_align;
    context->scoringParams = scoring;
    context->sbp = sbp;
    context->localScalingFactor = localScalingFactor;
    context->prog_number = program_number;

    gapping_params->gap_open = scoring->gap_open;
    gapping_params->gap_extend = scoring->gap_extend;
    gapping_params->decline_align = scoring->decline_align;
    /* YIKES! different x-dropoff due to different pass through the
       blast code */
    gapping_params->x_dropoff = gap_align->gap_x_dropoff;
    gapping_params->context = context;
}


static const Blast_RedoAlignCallbacks
redo_align_callbacks = {
    s_CalcLambda, s_SequenceGetRange, s_RedoOneAlignment,
    s_NewAlignmentUsingXdrop
};


static Blast_RedoAlignParams *
s_GetAlignParams(Blast_GappingParamsContext * context,
                 EBlastProgramType program_number,
                 BlastGapAlignStruct * gap_align,
                 BLAST_SequenceBlk * queryBlk,
                 BlastQueryInfo* queryInfo,
                 BlastScoreBlk* sbp,
                 BlastScoringParameters* scoringParams,
                 const BlastExtensionParameters* extendParams,
                 const BlastHitSavingParameters* hitParams,
                 const PSIBlastOptions* psiOptions,
                 const char * matrixName,
                 double localScalingFactor,
                 int adjustParameters)
{
    int rows;
    int cutoff_s;
    double cutoff_e;
    BlastCompo_GappingParams * gapping_params = NULL;
    Blast_MatrixInfo * scaledMatrixInfo;
    Blast_KarlinBlk* kbp;
    int subject_is_translated = program_number == eBlastTypeTblastn;
    /* YIKES! wrong test for do_link_hsps */
    int do_link_hsps = program_number == eBlastTypeTblastn;
    Boolean positionBased = (sbp->psi_matrix ? TRUE : FALSE);

    if (do_link_hsps) {
        ASSERT( 0 && "Which cutoff needed here?" );
        /*     cutoff_s = search->pbp->cutoff_s2 * localScalingFactor; */
    } else {
        /* There is no cutoff score; we consider e-values instead */
        cutoff_s = 0;
    }
    cutoff_e = hitParams->options->expect_value;
    rows = positionBased ? queryInfo->max_length : BLASTAA_SIZE;
    scaledMatrixInfo = Blast_MatrixInfoNew(rows, positionBased);
    s_MatrixInfoInit(scaledMatrixInfo, localScalingFactor,
                     queryBlk, queryInfo, sbp, scoringParams,
                     positionBased, matrixName);
    kbp = sbp->kbp_gap[0];
    gapping_params = malloc(sizeof(BlastCompo_GappingParams));
    s_GappingParamsInit(context, gapping_params, gap_align, scoringParams,
                        sbp, localScalingFactor, program_number,
                        kbp->Lambda);
    return
        Blast_RedoAlignParamsNew(&scaledMatrixInfo, &gapping_params,
                                 adjustParameters, positionBased,
                                 subject_is_translated,
                                 queryInfo->max_length, cutoff_s, cutoff_e,
                                 do_link_hsps, kbp->Lambda, kbp->logK,
                                 &redo_align_callbacks);
}


/**
 * Convert a BlastCompo_Heap to a flat list of SeqAligns. Note that
 * there may be more than one alignment per element in the heap.  The
 * new list preserves the order of the SeqAligns associated with each
 * HeapRecord. (@todo this function is named as it is for
 * compatibility with kappa.c, rename in the future)
 *
 * @param self           a BlastCompo_Heap
 * @param results        BLAST core external results structure (pre-SeqAlign)
 *                       [out]
 * @param hitlist_size   size of each list in the results structure above [in]
 */
static void
s_HeapToFlatList(BlastCompo_Heap * self, BlastHSPResults * results,
                 Int4 hitlist_size)
{
    BlastHSPList* hsp_list;
    BlastHitList* hitlist =
        results->hitlist_array[0] = Blast_HitListNew(hitlist_size);

    hsp_list = NULL;
    while (NULL != (hsp_list = BlastCompo_HeapPop(self))) {
        Blast_HitListUpdate(hitlist, hsp_list);
    }
}


/**
 *  Top level routine to recompute alignments for each
 *  match found by the gapped BLAST algorithm
 *
 *  @param search           is the structure with all the information about
 *                          the search
 *  @param options          is used to pass certain command line options
 *                          taken in by BLAST
 *  @param hitlist_count    is the number of old matches
 *  @param adjustParameters determines whether we are to adjust the
 *                          Karlin-Altschul parameters and score matrix
 *  @param SmithWaterman    determines whether the new local alignments
 *                          should be computed by the optimal Smith-Waterman
 *                          algorithm; SmithWaterman false means that
 *                          alignments will be recomputed by the current
 *                          X-drop algorithm as implemented in the procedure
 *                          ALIGN.
 *  @return                 a array of lists of SeqAlign; each element
 *                          in the array is a list of SeqAligns for
 *                          one query in the concatenated query.
 *  It is assumed that at least one of adjustParameters and
 *  SmithWaterman is >0 or true when this procedure is called A linked list
 *  of alignments is returned; the alignments are sorted according to
 *  the lowest E-value of the best alignment for each matching
 *  sequence; alignments for the same matching sequence are in the
 *  list consecutively regardless of the E-value of the secondary
 *  alignments. Ties in sorted order are much rarer than for the
 *  standard BLAST method, but are broken deterministically based on
 *  the index of the matching sequences in the database.
 */
Int2
Blast_RedoAlignmentCore(EBlastProgramType program_number,
                        BLAST_SequenceBlk * queryBlk,
                        BlastQueryInfo* queryInfo,
                        BlastScoreBlk* sbp,
                        BlastHSPStream* hsp_stream,
                        const BlastSeqSrc* seqSrc,
                        const Uint1* gen_code_string,
                        BlastScoringParameters* scoringParams,
                        const BlastExtensionParameters* extendParams,
                        const BlastHitSavingParameters* hitParams,
                        const PSIBlastOptions* psiOptions,
                        BlastHSPResults* results)
{
    double localScalingFactor;            /* the factor by which to
                                           * scale the scoring system in
                                           * order to obtain greater
                                           * precision */
    Int4      **matrix;                   /* score matrix */
    s_SearchParameters *searchParams; /* the values of the search
                                       * parameters that will be
                                       * recorded, altered in the
                                       * search structure in this
                                       * routine, and then restored
                                       * before the routine
                                       * exits. */
    Blast_ForbiddenRanges forbidden;    /* forbidden ranges for each
                                           * database position (used
                                           * in Smith-Waterman
                                           * alignments)
                                           */
    BlastCompo_Heap * redoneMatches;  /* a collection of alignments
                                       * for each query sequence with
                                       * sequences from the
                                       * database */
    Blast_CompositionWorkspace
        *NRrecord = NULL;        /* stores all fields needed for
                                  * computing a compositionally adjusted
                                  * score matrix using Newton's method */
    Int4 query_index;            /* loop index */
    Int4 numQueries;             /* number of queries in the
                                    concatenated query */
    BlastGapAlignStruct* gapAlign;        /* keeps track of gapped
                                             alignment params */
    double inclusion_ethresh;    /* All alignments above this value will be
                                    reported, no matter how many. */
    BlastCompo_QueryInfo * query_info = NULL;
    Blast_RedoAlignParams * redo_align_params;
    Boolean positionBased = (sbp->psi_matrix ? TRUE : FALSE);
    Boolean adjustParameters = extendParams->options->compositionBasedStats;
    Boolean SmithWaterman;
    int status_code;
    BlastHSPList* thisMatch = NULL;  /* alignment data for the
                                      * current query-subject
                                      * match */
    BlastCompo_Alignment * incoming_aligns;  /* existing algnments
                                                for a match */
    Blast_GappingParamsContext gapping_params_context;
    int do_link_hsps;

    /**** Validate parameters *************/
    if (0 == strcmp(scoringParams->options->matrix, "BLOSUM62_20") &&
       !adjustParameters) {
        return 0;                   /* BLOSUM62_20 only makes sense if
                                     * adjustParameters is on */
    }
    if (positionBased) {
        adjustParameters = adjustParameters ? 1 : 0;
    }
    if (extendParams->options->eTbackExt == eSmithWatermanTbck) {
        SmithWaterman = TRUE;
    } else {
        SmithWaterman = FALSE;
    }
    inclusion_ethresh =
        (psiOptions != NULL) ? psiOptions->inclusion_ethresh : 0;

    /*****************/
    /* Initialize searchParams */
    searchParams =
        s_SearchParametersNew(queryInfo->max_length, adjustParameters,
                              positionBased);
    s_RecordInitialSearch(searchParams, queryBlk, queryInfo, sbp,
                          scoringParams, queryInfo->max_length,
                          adjustParameters, positionBased);
    if (adjustParameters) {
        if((0 == strcmp(scoringParams->options->matrix, "BLOSUM62_20"))) {
            localScalingFactor = SCALING_FACTOR / 10;
        } else {
            localScalingFactor = SCALING_FACTOR;
        }
    } else {
        localScalingFactor = 1.0;
    }
    s_RescaleSearch(searchParams, queryBlk, queryInfo, sbp, scoringParams,
                    localScalingFactor, positionBased);
    /********/
    if (positionBased) {
        matrix = sbp->psi_matrix->pssm->data;
        if ( !matrix ) {
            /* YIKES! error return
               Char* msg =
               "Cannot perform position-specific search without a PSSM";
               BlastConstructErrorMessage("RedoAlignmentCore", msg, 3,
               &(search->error_return));
               return NULL;
            */
        }
    } else {
        matrix = sbp->matrix->data;
    }
    if ((status_code=BLAST_GapAlignStructNew(scoringParams,
                                             extendParams,
                                             BlastSeqSrcGetMaxSeqLen(seqSrc),
                                             sbp, &gapAlign)) != 0) {
        return status_code;
    }
    gapAlign->gap_x_dropoff =
        extendParams->gap_x_dropoff_final * localScalingFactor;
    redo_align_params =
        s_GetAlignParams(&gapping_params_context, program_number,
                         gapAlign, queryBlk, queryInfo,
                         sbp, scoringParams, extendParams, hitParams,
                         psiOptions, scoringParams->options->matrix,
                         localScalingFactor, adjustParameters);
    do_link_hsps = redo_align_params->do_link_hsps;

    s_GetQueryInfo(&query_info, &numQueries, queryBlk->sequence, queryInfo);
    if(SmithWaterman) {
        Blast_ForbiddenRangesInitialize(&forbidden, queryInfo->max_length);
    }
    redoneMatches = calloc(numQueries, sizeof(BlastCompo_Heap));
    for (query_index = 0;  query_index < numQueries;  query_index++) {
        BlastCompo_HeapInitialize(&redoneMatches[query_index],
                                  hitParams->options->hitlist_size,
                                  inclusion_ethresh);
    }
    if( adjustParameters > 1 && !positionBased ) {
        NRrecord = Blast_CompositionWorkspaceNew();
        Blast_CompositionWorkspaceInit(NRrecord,
                                       scoringParams->options->matrix);
    }
    while (BlastHSPStreamRead(hsp_stream, &thisMatch) != kBlastHSPStream_Eof) {
        /* for all matching sequences */
        BlastCompo_MatchingSequence matchingSeq; /* the data for a matching
                                                  * database sequence */
        BlastCompo_Alignment ** alignments; /* array of lists of
                                             * alignments for each
                                             * query to this subject */
        alignments = calloc(numQueries, sizeof(BlastCompo_Alignment *));

        if(thisMatch->hsp_array == NULL) {
            continue;
        }
        if (BlastCompo_EarlyTermination(thisMatch->best_evalue,
                                        redoneMatches, numQueries)) {
            break;
        }
        /* Get the sequence for this match */
        s_MatchingSequenceInitialize(&matchingSeq, program_number,
                                     seqSrc, gen_code_string, thisMatch->oid);
        incoming_aligns =
            s_ResultHspToDistinctAlign(queryInfo, thisMatch->hsp_array,
                                       thisMatch->hspcnt, localScalingFactor);
        if (SmithWaterman) {
            Blast_RedoOneMatchSmithWaterman(alignments,
                                            redo_align_params,
                                            incoming_aligns,
                                            thisMatch->hspcnt,
                                            &matchingSeq, query_info,
                                            numQueries, matrix,
                                            NRrecord, &forbidden,
                                            redoneMatches);
        } else {
            Blast_RedoOneMatch(alignments, redo_align_params,
                               incoming_aligns, thisMatch->hspcnt,
                               &matchingSeq, queryInfo->max_length,
                               query_info, numQueries, matrix,
                               NRrecord);
        }
        for (query_index = 0;  query_index < numQueries;  query_index++) {
            /* Loop over queries */
            if( alignments[query_index] != NULL) { /* alignments were found */
                double bestEvalue;   /* best evalue among alignments in the
                                        hitlist */
                Int4 bestScore;      /* best score among alignments in
                                        the hitlist */
                BlastHSPList * hsp_list; /* a hitlist containing the
                                          * newly-computed alignments */
                void * discardedAligns;
                hsp_list =
                    s_HSPListFromDistinctAlignments(&alignments[query_index],
                                                    matchingSeq.index);
                if (hsp_list->hspcnt > 1) {
                    s_HitlistReapContained(hsp_list->hsp_array,
                                           &hsp_list->hspcnt);
                }
                s_HitlistEvaluateAndPurge(&bestScore, &bestEvalue,
                                          hsp_list,
                                          matchingSeq.length,
                                          program_number, queryInfo,
                                          sbp, hitParams,
                                          do_link_hsps);
                if (bestEvalue <= hitParams->options->expect_value &&
                    BlastCompo_HeapWouldInsert(&redoneMatches[query_index],
                                               bestEvalue, bestScore,
                                               thisMatch->oid)) {
                    s_HSPListRescaleScores(hsp_list, redo_align_params->Lambda,
                                           redo_align_params->logK,
                                           localScalingFactor);

                    BlastCompo_HeapInsert(&redoneMatches[query_index],
                                          hsp_list, bestEvalue,
                                          bestScore, thisMatch->oid,
                                          &discardedAligns);
                    if (discardedAligns != NULL) {
                        Blast_HSPListFree(discardedAligns);
                    }
                } else { /* the best alignment is not significant */
                    Blast_HSPListFree(hsp_list);
                } /* end if the best alignment is significant */
            } /* end if any alignments were found */
        } /* end loop over queries */
        s_MatchingSequenceRelease(&matchingSeq);
        thisMatch = Blast_HSPListFree(thisMatch);
        sfree(alignments);
        BlastCompo_AlignmentsFree(&incoming_aligns, NULL);
    }
    /* end for all matching sequences */
    /* YIKES! handle multiple queries
       for (query_index = 0;  query_index < numQueries;  query_index++) {
       results[query_index] =
       BlastCompo_HeapToFlatList(&redoneMatches[query_index]);
       }
    */
    s_HeapToFlatList(&redoneMatches[0], results,
                     hitParams->options->hitlist_size);
    /* Clean up */
    free(query_info);
    Blast_RedoAlignParamsFree(&redo_align_params);
    for (query_index = 0;  query_index < numQueries;  query_index++) {
        BlastCompo_HeapRelease(&redoneMatches[query_index]);
    }
    sfree(redoneMatches); redoneMatches = NULL;
    if(SmithWaterman) {
        Blast_ForbiddenRangesRelease(&forbidden);
    }
    gapAlign = BLAST_GapAlignStructFree(gapAlign);
    s_RestoreSearch(searchParams, sbp, matrix, queryInfo->max_length,
                    scoringParams, positionBased, adjustParameters);
    s_SearchParametersFree(&searchParams);
    if (NULL != NRrecord) {
        Blast_CompositionWorkspaceFree(&NRrecord);
    }
    return 0;
}
