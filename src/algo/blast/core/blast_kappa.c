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
#include <algo/blast/core/ncbi_math.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_kappa.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/link_hsps.h>
#include <algo/blast/core/gencode_singleton.h>
#include "blast_psi_priv.h"
#include "blast_gapalign_priv.h"
#include "blast_hits_priv.h"
#include "blast_posit.h"

#include <algo/blast/composition_adjustment/nlm_linear_algebra.h>
#include <algo/blast/composition_adjustment/compo_heap.h>
#include <algo/blast/composition_adjustment/redo_alignment.h>
#include <algo/blast/composition_adjustment/matrix_frequency_data.h>
#include <algo/blast/composition_adjustment/unified_pvalues.h>

/* Define KAPPA_PRINT_DIAGNOSTICS to turn on printing of
 * diagnostic information from some routines. */

/** Compile-time option; if set to a true value, then blastp runs
    that use Blast_RedoAlignmentCore to compute the traceback will not
    SEG the subject sequence */
#ifndef KAPPA_BLASTP_NO_SEG_SEQUENCE
#define KAPPA_BLASTP_NO_SEG_SEQUENCE 0
#endif


/** Compile-time option; if set to a true value, then blastp runs
    that use Blast_RedoAlignmentCore to compute the traceback will not
    SEG the subject sequence */
#ifndef KAPPA_TBLASTN_NO_SEG_SEQUENCE
#define KAPPA_TBLASTN_NO_SEG_SEQUENCE 0
#endif


/**
 * Given a list of HSPs with (possibly) high-precision scores, rescale
 * the scores to have standard precision and set the scale-independent
 * bit scores.  This routine does *not* resort the list; it is assumed
 * that the list is already sorted according to e-values that have been
 * computed using the initial, higher-precision scores.
 *
 * @param hsp_list          the HSP list
 * @param logK              Karlin-Altschul statistical parameter [in]
 * @param lambda            Karlin-Altschul statistical parameter [in]
 * @param scoreDivisor      the value by which reported scores are to be
 */
static void
s_HSPListNormalizeScores(BlastHSPList * hsp_list,
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
 * Adjusts the E-values in a BLAST_HitList to be composites of
 * a composition-based P-value and a score/alignment-based P-value
 *
 * @param hsp_list           the hitlist whose E-values need to be adjusted
 * @param comp_p_value      P-value from sequence composition
 * @param seqSrc            a source of sequence data
 * @param subject_length    length of database sequence
 * @param query_context     info about this query context; needed when
 *                          multiple queries are being used
 * @param LambdaRatio       the ratio between the observed value of Lambda
 *                          and the predicted value of lambda (used to print
 *                          diagnostics)
 * @param subject_id        the subject id of this sequence (used to print
 *                          diagnostics)
 **/
static void
s_AdjustEvaluesForComposition(
    BlastHSPList *hsp_list,
    double comp_p_value,
    const BlastSeqSrc* seqSrc,
    Int4 subject_length,
    BlastContextInfo * query_context,
    double LambdaRatio,
    int subject_id)
{
    /* Smallest observed evalue after adjustment */
    double best_evalue = DBL_MAX;

    /* True length of the query */
    int query_length =      query_context->query_length;
    /* Length adjustment to compensate for edge effects */
    int length_adjustment = query_context->length_adjustment;

    /* Effective lengths of the query, subject, and database */
    double query_eff   = MAX((query_length - length_adjustment), 1);
    double subject_eff = MAX((subject_length - length_adjustment), 1.0);
    double dblen_eff = (double) query_context->eff_searchsp / query_eff;
    
    /* Scale factor to convert the database E-value to the sequence E-value */
    double db_to_sequence_scale = subject_eff / dblen_eff;

    int        hsp_index;
    for (hsp_index = 0;  hsp_index < hsp_list->hspcnt;  hsp_index++) {
        /* for all HSPs */
        double align_p_value;     /* P-value for the alignment score */
        double combined_p_value;  /* combination of two P-values */

        /* HSP for this iteration */
        BlastHSP * hsp = hsp_list->hsp_array[hsp_index];
#ifdef KAPPA_PRINT_DIAGNOSTICS
        /* Original E-value, saved if diagnostics are printed. */
        double old_e_value = hsp->evalue;
#endif
        hsp->evalue *= db_to_sequence_scale;

        align_p_value = BLAST_KarlinEtoP(hsp->evalue);
        combined_p_value = Blast_Overall_P_Value(comp_p_value,align_p_value);
        hsp->evalue = BLAST_KarlinPtoE(combined_p_value);
        hsp->evalue /= db_to_sequence_scale;

        if (hsp->evalue < best_evalue) {
            best_evalue = hsp->evalue;
        }

#ifdef KAPPA_PRINT_DIAGNOSTICS
        {{
            int    sequence_gi; /*GI of a sequence*/
            Blast_GiList* gi_list; /*list of GI's for a sequence*/
            gi_list = BlastSeqSrcGetGis(seqSrc, (void *) (&subject_id));
            if ((gi_list) && (gi_list->num_used > 0)) {
                sequence_gi = gi_list->data[0];
            } else {
                sequence_gi = (-1);
            }
            printf("GI %d Lambda ratio %e comp. p-value %e; "
                   "adjust E-value of query length %d match length "
                   "%d from %e to %e\n",
                   sequence_gi, LambdaRatio, comp_p_value,
                   query_length, subject_length, old_e_value, hsp->evalue);
            Blast_GiListFree(gi_list);
        }}
#endif
    } /* end for all HSPs */

    hsp_list->best_evalue = best_evalue;

    /* suppress unused parameter warnings if diagnostics are not printed */
    (void) seqSrc;
    (void) query_length;
    (void) LambdaRatio;
    (void) subject_id;
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
s_HitlistReapContained(BlastHSP * hsp_array[], Int4 * hspcnt)
{
    Int4 iread;       /* iteration index used to read the hitlist */
    Int4 iwrite;      /* iteration index used to write to the hitlist */
    Int4 old_hspcnt;  /* number of HSPs in the hitlist on entry */

    old_hspcnt = *hspcnt;

    for (iread = 1;  iread < *hspcnt;  iread++) {
        /* for all HSPs in the hitlist */
        Int4      ireadBack;  /* iterator over indices less than iread */
        BlastHSP *hsp1;       /* an HSP that is a candidate for deletion */

        hsp1 = hsp_array[iread];
        for (ireadBack = 0;  ireadBack < iread && hsp1 != NULL;  ireadBack++) {
            /* for all HSPs before hsp1 in the hitlist and while hsp1
             * has not been deleted */
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


/** A callback used to free an EditScript that has been stored in a
 * BlastCompo_Alignment. */
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
 * @param queryInfo  information about all queries in this search [in]
 * @return Allocated and filled BlastHSPList structure.
 */
static BlastHSPList *
s_HSPListFromDistinctAlignments(BlastCompo_Alignment ** alignments,
                                int oid,
                                BlastQueryInfo* queryInfo)
{
    int status = 0;                    /* return code for any routine called */
    static const int unknown_value = 0;   /* dummy constant to use when a
                                      parameter value is not known */
    BlastCompo_Alignment * align;  /* an alignment in the list */
    BlastHSPList * hsp_list;       /* the new HSP list */

    hsp_list = Blast_HSPListNew(0);
    if (hsp_list == NULL) {
        return NULL;
    }
    hsp_list->oid = oid;

    for (align = *alignments;  NULL != align;  align = align->next) {
        BlastHSP * new_hsp = NULL;
        GapEditScript * editScript = align->context;
        int query_offset, queryStart, queryEnd;
        align->context = NULL;
        
        query_offset = queryInfo->contexts[align->queryIndex].query_offset;
        queryStart = align->queryStart - query_offset;
        queryEnd = align->queryEnd - query_offset;

        status = Blast_HSPInit(queryStart, queryEnd,
                               align->matchStart, align->matchEnd,
                               unknown_value, unknown_value,
                               align->queryIndex, 
                               0, (Int2) align->frame, align->score,
                               &editScript, &new_hsp);
        switch (align->matrix_adjust_rule) {
        case eDontAdjustMatrix:
            new_hsp->comp_adjustment_method = eNoCompositionBasedStats;
            break;
        case eCompoScaleOldMatrix:
            new_hsp->comp_adjustment_method = eCompositionBasedStats;
            break;
        default:
            new_hsp->comp_adjustment_method = eCompositionMatrixAdjust;
            break;
        }
        if (status != 0)
            break;
        /* At this point, the subject and possibly the query sequence have
         * been filtered; since it is not clear that num_ident of the
         * filtered sequences, rather than the original, is desired,
         * explicitly leave num_ident blank. */
        new_hsp->num_ident = 0;

        status = Blast_HSPListSaveHSP(hsp_list, new_hsp);
        if (status != 0)
            break;
    }
    if (status == 0) {
        BlastCompo_AlignmentsFree(alignments, s_FreeEditScript);
        Blast_HSPListSortByScore(hsp_list);
    } else {
        hsp_list = Blast_HSPListFree(hsp_list);
    }
    return hsp_list;
}


/**
 * Adding evalues to a list of HSPs and remove those that do not have
 * sufficiently good (low) evalue.
 *
 * @param *pbestScore      best (highest) score in the list
 * @param *pbestEvalue     best (lowest) evalue in the list
 * @param hsp_list         the list
 * @param seqSrc            a source of sequence data
 * @param subject_length   length of the subject sequence
 * @param program_number   the type of BLAST search being performed
 * @param queryInfo        information about the queries
 * @param query_index      the index of the query corresponding to 
 *                         the HSPs in hsp_list
 * @param sbp              the score block for this search
 * @param hitParams        parameters used to assign evalues and
 *                         decide whether to save hits.
 * @param pvalueForThisPair  composition p-value
 * @param LambdaRatio        lambda ratio, if available
 * @param subject_id         index of subject
 *
 * @return 0 on success; -1 on failure (can fail because some methods
 *         of generating evalues use auxiliary structures)
 */
static int
s_HitlistEvaluateAndPurge(int * pbestScore, double *pbestEvalue,
                          BlastHSPList * hsp_list,
                          const BlastSeqSrc* seqSrc,
                          int subject_length,
                          EBlastProgramType program_number,
                          BlastQueryInfo* queryInfo,
                          int query_index,
                          BlastScoreBlk* sbp,
                          const BlastHitSavingParameters* hitParams,
                          double pvalueForThisPair,
                          double LambdaRatio,
                          int subject_id)
{
    int status = 0;
    *pbestEvalue = DBL_MAX;
    *pbestScore  = 0;
    if (hitParams->do_sum_stats) {
        status = BLAST_LinkHsps(program_number, hsp_list, queryInfo,
                                subject_length, sbp,
                                hitParams->link_hsp_params, TRUE);
    } else {
        status =
            Blast_HSPListGetEvalues(queryInfo, hsp_list, TRUE, sbp,
                                    0.0, /* use a non-zero gap decay
                                            only when linking HSPs */
                                    1.0); /* Use scaling factor equal to
                                             1, because both scores and
                                             Lambda are scaled, so they
                                             will cancel each other. */
    }
    if (eBlastTypeBlastp == program_number) {
        if ((0 <= pvalueForThisPair) && (pvalueForThisPair <= 1)) {
            s_AdjustEvaluesForComposition(hsp_list, pvalueForThisPair, seqSrc,
                                          subject_length,
                                          &queryInfo->contexts[query_index],
                                          LambdaRatio, subject_id);
        }
    }
    if (status == 0) {
        Blast_HSPListReapByEvalue(hsp_list, hitParams->options);
        if (hsp_list->hspcnt > 0) {
            *pbestEvalue = hsp_list->best_evalue;
            *pbestScore  = hsp_list->hsp_array[0]->score;
        }
    }
    return status == 0 ? 0 : -1;
}

/** Compute the number of identities for the HSPs in the hsp_list
 * @note Should work for blastp and tblastn now.
 * 
 * @param query_blk the query sequence data [in]
 * @param query_info structure describing the query_blk structure [in]
 * @param seq_src source of subject sequence data [in]
 * @param hsp_list list of HSPs to be processed [in|out]
 * @param scoring_options scoring options [in]
 * @gen_code_string Genetic code for tblastn [in]
 */
static void
s_ComputeNumIdentities(const BLAST_SequenceBlk* query_blk,
                       const BlastQueryInfo* query_info,
                       const BlastSeqSrc* seq_src,
                       BlastHSPList* hsp_list,
                       const BlastScoringOptions* scoring_options,
                       const Uint1* gen_code_string)
{
    Uint1* query = NULL;
    Uint1* query_nomask = NULL;
    Uint1* subject = NULL;
    const EBlastProgramType program_number = scoring_options->program_number;
    const Boolean kIsOutOfFrame = scoring_options->is_ooframe;
    const EBlastEncoding encoding = Blast_TracebackGetEncoding(program_number);
    BlastSeqSrcGetSeqArg seq_arg;
    BLAST_SequenceBlk* subject_blk = NULL;
    Int2 status = 0;
    int i;
    SBlastTargetTranslation* target_t = NULL;

    if ( !hsp_list || (program_number != eBlastTypeBlastp && program_number != eBlastTypeTblastn)) 
        return;

    /* Initialize the subject */
    {
        memset((void*) &seq_arg, 0, sizeof(seq_arg));
        seq_arg.oid = hsp_list->oid;
        seq_arg.encoding = encoding;
        seq_arg.check_oid_exclusion = TRUE;
        status = BlastSeqSrcGetSequence(seq_src, (void*) &seq_arg);
        ASSERT(status == 0);
    }

    if (program_number == eBlastTypeTblastn) {
        subject_blk = seq_arg.seq;
	BlastTargetTranslationNew(subject_blk, gen_code_string, eBlastTypeTblastn,
            kIsOutOfFrame, &target_t);
    }
    else {
        subject = seq_arg.seq->sequence;
    }

    for (i = 0; i < hsp_list->hspcnt; i++) {
        BlastHSP* hsp = hsp_list->hsp_array[i];

        /* Initialize the query */
        if (program_number == eBlastTypeBlastx && kIsOutOfFrame) {
            Int4 context = hsp->context - hsp->context % CODON_LENGTH;
            Int4 context_offset = query_info->contexts[context].query_offset;
            query = query_blk->oof_sequence + CODON_LENGTH + context_offset;
            query_nomask = query_blk->oof_sequence + CODON_LENGTH + context_offset;
        } else {
            query = query_blk->sequence + 
                query_info->contexts[hsp->context].query_offset;
            query_nomask = query_blk->sequence_nomask + 
                query_info->contexts[hsp->context].query_offset;
        }

        /* Translate subject if needed. */
        if (program_number == eBlastTypeTblastn) {
            const Uint1* target_sequence = Blast_HSPGetTargetTranslation(target_t, hsp, NULL);
            status = Blast_HSPGetNumIdentities(query, target_sequence, hsp, scoring_options, 0);
        }
        else
            status = Blast_HSPGetNumIdentities(query_nomask, subject, hsp, scoring_options, 0);

        ASSERT(status == 0);
    }
    target_t = BlastTargetTranslationFree(target_t);
    BlastSeqSrcReleaseSequence(seq_src, (void*) &seq_arg);
    BlastSequenceBlkFree(seq_arg.seq);
}


/**
 * A callback routine: compute lambda for the given score
 * probabilities.
 * (@sa calc_lambda_type).
 */
static double
s_CalcLambda(double probs[], int min_score, int max_score, double lambda0)
{
   
    int i;                 /* loop index */      
    int score_range;       /* range of possible scores */
    double avg;            /* expected score of aligning two characters */
    Blast_ScoreFreq freq;  /* score frequency data */

    score_range = max_score - min_score + 1;
    avg = 0.0;
    for (i = 0;  i < score_range;  i++) {
        avg += (min_score + i) * probs[i];
    }
    freq.score_min = min_score;
    freq.score_max = max_score;
    freq.obs_min = min_score;
    freq.obs_max = max_score;
    freq.sprob0 = probs;
    freq.sprob = &probs[-min_score];
    freq.score_avg = avg;

    return Blast_KarlinLambdaNR(&freq, lambda0);
}


/** Fill a two-dimensional array with the frequency ratios that
 * underlie a position specific score matrix (PSSM).
 *
 * @param returnRatios     a two-dimensional array with BLASTAA_SIZE
 *                         columns
 * @param numPositions     the number of rows in returnRatios
 * @param query            query sequence data, of length numPositions
 * @param matrixName       the name of the position independent matrix
 *                         corresponding to this PSSM
 * @param startNumerator   position-specific data used to generate the
 *                         PSSM
 * @return   0 on success; -1 if the named matrix isn't known, or if
 *           there was a memory error
 * @todo find out what start numerator is.
 */
static int
s_GetPosBasedStartFreqRatios(double ** returnRatios,
                             Int4 numPositions,
                             Uint1 * query,
                             const char *matrixName,
                             double **startNumerator)
{
    Int4 i,j;                            /* loop indices */
    SFreqRatios * stdFreqRatios = NULL;  /* frequency ratios for the
                                            named matrix. */
    double *standardProb;                /* probabilities of each
                                            letter*/
    const double kPosEpsilon = 0.0001;   /* values below this cutoff
                                            are treated specially */

    stdFreqRatios = _PSIMatrixFrequencyRatiosNew(matrixName);
    if (stdFreqRatios == NULL) {
        return -1;
    }
    for (i = 0;  i < numPositions;  i++) {
        for (j = 0;  j < BLASTAA_SIZE;  j++) {
            returnRatios[i][j] = stdFreqRatios->data[query[i]][j];
        }
    }
    stdFreqRatios = _PSIMatrixFrequencyRatiosFree(stdFreqRatios);

    standardProb = BLAST_GetStandardAaProbabilities();
    if(standardProb == NULL) {
        return -1;
    }
    /*reverse multiplication done in posit.c*/
    for (i = 0;  i < numPositions;  i++) {
        for (j = 0;  j < BLASTAA_SIZE;  j++) {
            if ((standardProb[query[i]] > kPosEpsilon) &&
                (standardProb[j] > kPosEpsilon) &&
                (j != eStopChar) && (j != eXchar) &&
                (startNumerator[i][j] > kPosEpsilon)) {
                returnRatios[i][j] = startNumerator[i][j] / standardProb[j];
            }
        }
    }
    sfree(standardProb);

    return 0;
}


/**
 * Fill a two-dimensional array with the frequency ratios that underlie the
 * named score matrix.
 *
 * @param returnRatios  a two-dimensional array of size
 *                      BLASTAA_SIZE x  BLASTAA_SIZE
 * @param matrixName    the name of a matrix
 * @return   0 on success; -1 if the named matrix isn't known, or if
 *           there was a memory error
 */
static int
s_GetStartFreqRatios(double ** returnRatios,
                     const char *matrixName)
{
    /* Loop indices */
    int i,j;
    /* Frequency ratios for the matrix */
    SFreqRatios * stdFreqRatios = NULL;

    stdFreqRatios = _PSIMatrixFrequencyRatiosNew(matrixName);
    if (stdFreqRatios == NULL) {
        return -1;
    }
    for (i = 0;  i < BLASTAA_SIZE;  i++) {
        for (j = 0;  j < BLASTAA_SIZE;  j++) {
            returnRatios[i][j] = stdFreqRatios->data[i][j];
        }
    }
    stdFreqRatios = _PSIMatrixFrequencyRatiosFree(stdFreqRatios);

    return 0;
}


/** SCALING_FACTOR is a multiplicative factor used to get more bits of
 * precision in the integer matrix scores. It cannot be arbitrarily
 * large because we do not want total alignment scores to exceed
 * -(BLAST_SCORE_MIN) */
#define SCALING_FACTOR 32


/**
 * Produce a scaled-up version of the position-specific matrix
 * with a given set of position-specific residue frequencies.
 *
 * @param fillPosMatrix     is the matrix to be filled
 * @param matrixName        name of the standard substitution matrix [in]
 * @param posFreqs          PSSM's frequency ratios [in]
 * @param query             Query sequence data [in]
 * @param queryLength       Length of the query sequence above [in]
 * @param sbp               stores various parameters of the search
 * @param scale_factor      amount by which ungapped parameters should be
 *                          scaled.
 * @return 0 on success; -1 on failure
 */
static int
s_ScalePosMatrix(int ** fillPosMatrix,
                 const char * matrixName,
                 double ** posFreqs,
                 Uint1 * query,
                 int queryLength,
                 BlastScoreBlk* sbp,
                 double scale_factor)
{
    /* Data used by scaling routines */    
    Kappa_posSearchItems *posSearch = NULL;
    /* A reduced collection of search parameters used by PSI-blast */
    Kappa_compactSearchItems *compactSearch = NULL;
    /* Representation of a PSSM internal to PSI-blast */
    _PSIInternalPssmData* internal_pssm = NULL;
    /* return code */
    int status = 0;

    posSearch = Kappa_posSearchItemsNew(queryLength, matrixName,
                                        fillPosMatrix, posFreqs);
    compactSearch = Kappa_compactSearchItemsNew(query, queryLength, sbp);
    /* Copy data into new structures */
    internal_pssm = _PSIInternalPssmDataNew(queryLength, BLASTAA_SIZE);
    if (posSearch == NULL || compactSearch == NULL || internal_pssm == NULL) {
        status = -1;
        goto cleanup;
    }
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
    if (status != 0) {
        goto cleanup;
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
                                 scale_factor, FALSE, sbp);
cleanup:
    internal_pssm = _PSIInternalPssmDataFree(internal_pssm);
    posSearch = Kappa_posSearchItemsFree(posSearch);
    compactSearch = Kappa_compactSearchItemsFree(compactSearch);

    return status;
}


/**
 * Convert an array of HSPs to a list of BlastCompo_Alignment objects.
 * The context field of each BlastCompo_Alignment is set to point to the
 * corresponding HSP.
 *
 * @param hsp_array             an array of HSPs
 * @param hspcnt                the length of hsp_array
 * @param queryInfo            information about the concatenated query
 * @param localScalingFactor    the amount by which this search is scaled
 *
 * @return the new list of alignments; or NULL if there is an out-of-memory
 *         error (or if the original array is empty)
 */
static BlastCompo_Alignment *
s_ResultHspToDistinctAlign(BlastHSP * hsp_array[], Int4 hspcnt,
                           BlastQueryInfo* queryInfo,
                           double localScalingFactor)
{
    BlastCompo_Alignment * aligns = NULL;      /* new list of alignments */
    BlastCompo_Alignment * tail = NULL;        /* last element in aligns */
    int hsp_index;                             /* loop index */
    
    for (hsp_index = 0;  hsp_index < hspcnt;  hsp_index++) {
        BlastHSP * hsp = hsp_array[hsp_index]; /* current HSP */
        BlastCompo_Alignment * new_align;      /* newly-created alignment */
        int queryStart;                        /* start of the query context
                                                  in the concatenated query */
        int queryOffset, queryEnd;             /* coordinates of the
                                                  query portion of the
                                                  concatenated query */
        /* Incoming alignments will have coordinates of the query
           portion relative to a particular query context; they must
           be shifted for used in the composition_adjustment library.
        */
        queryStart = queryInfo->contexts[hsp->context].query_offset;
        queryOffset = hsp->query.offset + queryStart;
        queryEnd = hsp->query.end + queryStart;
        new_align =
            BlastCompo_AlignmentNew((int) (hsp->score * localScalingFactor),
                                    eDontAdjustMatrix,
                                    queryOffset, queryEnd, hsp->context,
                                    hsp->subject.offset, hsp->subject.end,
                                    hsp->subject.frame, hsp);
        if (new_align == NULL) /* out of memory */
            goto error_return;
        if (tail == NULL) { /* if the list aligns is empty; */
            /* make new_align the first element in the list */
            aligns = new_align;
        } else {
            /* otherwise add new_align to the end of the list */
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
 * usually be the same as the S-W alignment. The call to ALIGN_EX
 * attempts to force the endpoints of the alignment to match the
 * optimal endpoints determined by the Smith-Waterman algorithm.
 * ALIGN_EX is used, so that if the data structures for storing BLAST
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
 * @param gap_align     parameters for a gapped alignment
 * @param scoringParams Settings for gapped alignment.[in]
 * @param score         score computed by the Smith-Waterman algorithm
 * @param queryAlignmentExtent  length of the alignment in the query sequence,
 *                              as computed by the x-drop algorithm
 * @param matchAlignmentExtent  length of the alignment in the subject
 *                              sequence, as computed by the x-drop algorithm
 * @param newScore              alignment score computed by the x-drop
 *                              algorithm
 */
static void
s_SWFindFinalEndsUsingXdrop(BlastCompo_SequenceData * query,
                            Int4 queryStart,
                            Int4 queryEnd,
                            BlastCompo_SequenceData * subject,
                            Int4 matchStart,
                            Int4 matchEnd,
                            BlastGapAlignStruct* gap_align,
                            const BlastScoringParameters* scoringParams,
                            Int4 score,
                            Int4 * queryAlignmentExtent,
                            Int4 * matchAlignmentExtent,
                            Int4 * newScore)
{
    Int4 XdropAlignScore;         /* alignment score obtained using X-dropoff
                                   * method rather than Smith-Waterman */
    Int4 doublingCount = 0;       /* number of times X-dropoff had to be
                                   * doubled */
    Int4 gap_x_dropoff_orig = gap_align->gap_x_dropoff;

    GapPrelimEditBlockReset(gap_align->rev_prelim_tback);
    GapPrelimEditBlockReset(gap_align->fwd_prelim_tback);
    do {
        XdropAlignScore =
            ALIGN_EX(&(query->data[queryStart]) - 1,
                     &(subject->data[matchStart]) - 1,
                     queryEnd - queryStart + 1, matchEnd - matchStart + 1,
                     queryAlignmentExtent,
                     matchAlignmentExtent, gap_align->fwd_prelim_tback,
                     gap_align, scoringParams, queryStart - 1, FALSE, FALSE,
                     NULL);

        gap_align->gap_x_dropoff *= 2;
        doublingCount++;
        if((XdropAlignScore < score) && (doublingCount < 3)) {
            GapPrelimEditBlockReset(gap_align->fwd_prelim_tback);
        }
    } while((XdropAlignScore < score) && (doublingCount < 3));

    gap_align->gap_x_dropoff = gap_x_dropoff_orig;
    *newScore = XdropAlignScore;
}


/**
 * BLAST-specific information that is associated with a
 * BlastCompo_MatchingSequence.
 */
typedef struct
BlastKappa_SequenceInfo {
    EBlastProgramType prog_number; /**< identifies the type of blast
                                        search being performed. The type
                                        of search determines how sequence
                                        data should be obtained. */
    const BlastSeqSrc* seq_src;    /**< BLAST sequence data source */
    BlastSeqSrcGetSeqArg seq_arg;  /**< argument to GetSequence method
                                     of the BlastSeqSrc (@todo this
                                     structure was designed to be
                                     allocated on the stack, i.e.: in
                                     Kappa_MatchingSequenceInitialize) */
} BlastKappa_SequenceInfo;


/** Release the resources associated with a matching sequence. */
static void
s_MatchingSequenceRelease(BlastCompo_MatchingSequence * self)
{
    if (self != NULL) {
        BlastKappa_SequenceInfo * local_data = self->local_data;
        BlastSeqSrcReleaseSequence(local_data->seq_src,
                                   &local_data->seq_arg);
        BlastSequenceBlkFree(local_data->seq_arg.seq);
        free(self->local_data);
        self->local_data = NULL;
    }
}

/**
 * Test whether the aligned parts of two sequences that
 * have a high-scoring gapless alignment are nearly identical
 *
 * @param seqData           subject sequence
 * @param queryData         query sequence
 * @param queryOffset       offset for align if there are multiple queries
 * @param align             information about the alignment
 * @param rangeOffset       offset for subject sequence (used for tblastn)
 *
 * @return                  TRUE if the aligned portions are nearly identical
 */
static Boolean 
s_TestNearIdentical(const BlastCompo_SequenceData *seqData, 
		 const BlastCompo_SequenceData *queryData, 
		    const int queryOffset,   
		 const BlastCompo_Alignment *align,
		 const int rangeOffset)
{
  int numIdentical = 0;
  double fractionIdentical;
  int qPos, sPos; /*positions in query and subject;*/
  int qEnd; /*end of query*/
  const double kMinFractionNearIdentical = 0.98; /* cutoff for this check. */

  qPos = align->queryStart - queryOffset;
  qEnd = align->queryEnd - queryOffset;
  sPos = align->matchStart - rangeOffset;
  while (qPos < qEnd)  {
      if (queryData->data[qPos] == seqData->data[sPos])
	numIdentical++;
      sPos++;
      qPos++;
  }
  fractionIdentical = ((double) numIdentical/
  (double) (align->queryEnd - align->queryStart));
  if (fractionIdentical >= kMinFractionNearIdentical)
    return(TRUE);
  else
    return(FALSE);
}


/**
 * Initialize a new matching sequence, obtaining information about the
 * sequence from the search.
 *
 * @param self              object to be initialized
 * @param seqSrc            A pointer to a source from which sequence data
 *                          may be obtained
 * @param program_number    identifies the type of blast search being
 *                          performed.
 * @param default_db_genetic_code   default genetic code to use when
 *                          subject sequences are translated and there is
 *                          no other guidance on what code to use
 * @param subject_index     index of the matching sequence in the database
 */
static int
s_MatchingSequenceInitialize(BlastCompo_MatchingSequence * self,
                             EBlastProgramType program_number,
                             const BlastSeqSrc* seqSrc,
                             Int4 default_db_genetic_code,
                             Int4 subject_index)
{
    BlastKappa_SequenceInfo * seq_info;  /* BLAST-specific sequence
                                            information */
    self->length = 0;
    self->local_data = NULL;

    seq_info = malloc(sizeof(BlastKappa_SequenceInfo));
    if (seq_info != NULL) {
        self->local_data = seq_info;

        seq_info->seq_src      = seqSrc;
        seq_info->prog_number  = program_number;

        memset((void*) &seq_info->seq_arg, 0, sizeof(seq_info->seq_arg));
        seq_info->seq_arg.oid = self->index = subject_index;
        seq_info->seq_arg.check_oid_exclusion = TRUE;

        if( program_number == eBlastTypeTblastn ) {
            seq_info->seq_arg.encoding = eBlastEncodingNcbi4na;
        } else {
            seq_info->seq_arg.encoding = eBlastEncodingProtein;
        }
        if (BlastSeqSrcGetSequence(seqSrc, &seq_info->seq_arg) >= 0) {
            self->length =
                BlastSeqSrcGetSeqLen(seqSrc, (void*) &seq_info->seq_arg);

            /* If the subject is translated and the BlastSeqSrc implementation
             * doesn't provide a genetic code string, use the default genetic
             * code for all subjects (as in the C toolkit) */
            if (Blast_SubjectIsTranslated(program_number) &&
                seq_info->seq_arg.seq->gen_code_string == NULL) {
                seq_info->seq_arg.seq->gen_code_string =
                             GenCodeSingletonFind(default_db_genetic_code);
                ASSERT(seq_info->seq_arg.seq->gen_code_string);
            }
        } else {
            self->length = 0;
        }
    }
    if (self->length == 0) {
        /* Could not obtain the required data */
        s_MatchingSequenceRelease(self);
        return -1;
    } else {
        return 0;
    }
}


/** NCBIstdaa encoding for 'X' character */
#define BLASTP_MASK_RESIDUE 21
/** Default instructions and mask residue for SEG filtering */
#define BLASTP_MASK_INSTRUCTIONS "S 10 1.8 2.1"


/**
 * Filter low complexity regions from the sequence data; uses the SEG
 * algorithm.
 *
 * @param seqData            data to be filtered
 * @param program_name       type of search being performed
 * @return   0 for success; -1 for out-of-memory
 */
static int
s_DoSegSequenceData(BlastCompo_SequenceData * seqData,
                    EBlastProgramType program_name)
{
    int status = 0;
    BlastSeqLoc* mask_seqloc = NULL;
    SBlastFilterOptions* filter_options = NULL;

    status = BlastFilteringOptionsFromString(program_name,
                                             BLASTP_MASK_INSTRUCTIONS,
                                             &filter_options, NULL);
    if (status == 0) {
        status = BlastSetUp_Filter(program_name, seqData->data,
                                   seqData->length, 0, filter_options,
                                   &mask_seqloc, NULL);
        filter_options = SBlastFilterOptionsFree(filter_options);
    }
    if (status == 0) {
        Blast_MaskTheResidues(seqData->data, seqData->length,
                              FALSE, mask_seqloc, FALSE, 0);
    }
    if (mask_seqloc != NULL) {
        mask_seqloc = BlastSeqLocFree(mask_seqloc);
    }
    return status;
}


/**
 * Obtain a string of translated data
 *
 * @param self          the sequence from which to obtain the data [in]
 * @param range         the range and translation frame to get [in]
 * @param seqData       the resulting data [out]
 * @param queryData     the query sequence [in]
 * @param queryOffset   offset for align if there are multiple queries
 * @param align          information about the alignment between query and subject
 * @param shouldTestIdentical did alignment pass a preliminary test in
 *                       redo_alignment.c that indicates the sequence
 *                        pieces may be near identical
 *
 * @return 0 on success; -1 on failure
 */
static int
s_SequenceGetTranslatedRange(const BlastCompo_MatchingSequence * self,
                             const BlastCompo_SequenceRange * range,
                             BlastCompo_SequenceData * seqData,
			     const BlastCompo_SequenceData * queryData,
			     const int queryOffset,
			     const BlastCompo_Alignment *align,
			     const Boolean shouldTestIdentical)
{
    int status = 0;
    BlastKappa_SequenceInfo * local_data; /* BLAST-specific
                                             information associated
                                             with the sequence */
    Uint1 * translation_buffer;           /* a buffer for the translated,
                                             amino-acid sequence */
    Int4 translated_length;  /* length of the translated sequence */
    int translation_frame;   /* frame in which to translate */
    Uint1 * na_sequence;     /* the nucleotide sequence */
    int translation_start;   /* location in na_sequence to start
                                translating */
    int num_nucleotides;     /* the number of nucleotides to be translated */
    
    local_data = self->local_data;
    na_sequence = local_data->seq_arg.seq->sequence_start;

    /* Initialize seqData to nil, in case this routine fails */
    seqData->buffer = NULL;
    seqData->data = NULL;
    seqData->length = 0;

    translation_frame = range->context;
    if (translation_frame > 0) {
        translation_start = 3 * range->begin;
    } else {
        translation_start =
            self->length - 3 * range->end + translation_frame + 1;
    }
    num_nucleotides =
        3 * (range->end - range->begin) + ABS(translation_frame) - 1;
    
    status = Blast_GetPartialTranslation(na_sequence + translation_start,
                                         num_nucleotides,
                                         (Int2) translation_frame,
                                         local_data->seq_arg.seq->gen_code_string,
                                         &translation_buffer,
                                         &translated_length,
                                         NULL);
    if (status == 0) {
        seqData->buffer = translation_buffer;
        seqData->data   = translation_buffer + 1;
        seqData->length = translated_length;

        if ( !(KAPPA_TBLASTN_NO_SEG_SEQUENCE) ) {
	  if ((!shouldTestIdentical) || 
            (shouldTestIdentical && 
	     (!s_TestNearIdentical(seqData, queryData, queryOffset, align, range->begin)))) {
            status = s_DoSegSequenceData(seqData, eBlastTypeTblastn);
            if (status != 0) {
                free(seqData->buffer);
                seqData->buffer = NULL;
                seqData->data = NULL;
                seqData->length = 0;
            }
	  }
        }
    }
    return status;
}


/**
 * Get a string of protein data from a protein sequence.
 *
 * @param self          a protein sequence [in]
 * @param range         the range to get [in]
 * @param seqData       the resulting data [out]
 * @param queryData     the query sequence [in]
 * @param queryOffset   offset for align if there are multiple queries
 * @param align          information about the alignment 
 *                         between query and subject [in]
 * @param shouldTestIdentical did alignment pass a preliminary test in
 *                       redo_alignment.c that indicates the sequence
 *                        pieces may be near identical [in]
 *
 * @return 0 on success; -1 on failure
 */
static int
s_SequenceGetProteinRange(const BlastCompo_MatchingSequence * self,
                          const BlastCompo_SequenceRange * range,
                          BlastCompo_SequenceData * seqData,
			  const BlastCompo_SequenceData * queryData,
			  const int queryOffset,
			  const BlastCompo_Alignment *align,
			  const Boolean shouldTestIdentical)

{
    int status = 0;       /* return status */
    Int4       idx;       /* loop index */
    Uint1     *origData;  /* the unfiltered data for the sequence */
    /* BLAST-specific sequence information */
    BlastKappa_SequenceInfo * local_data = self->local_data;

    seqData->data = NULL;
    seqData->length = 0;
    /* Copy the entire sequence (necessary for SEG filtering.) */
    seqData->buffer  = calloc((self->length + 2), sizeof(Uint1));
    if (seqData->buffer == NULL) {
        return -1;
    }
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
    if ( !(KAPPA_BLASTP_NO_SEG_SEQUENCE) ) {
      if ((!shouldTestIdentical) || 
	  (shouldTestIdentical && 
	   (!s_TestNearIdentical(seqData, queryData, queryOffset, align, 0)))) {
	status = -1;
        status = s_DoSegSequenceData(seqData, eBlastTypeBlastp);
      }
    }
    /* Fit the data to the range. */
    seqData ->data    = &seqData->data[range->begin - 1];
    *seqData->data++  = '\0';
    seqData ->length  = range->end - range->begin;

    if (status != 0) {
        free(seqData->buffer);
        seqData->buffer = NULL;
        seqData->data = NULL;
    }
    return status;
}


/**
 * Obtain the sequence data that lies within the given range.
 *
 * @param self          sequence information [in]
 * @param range        range specifying the range of data [in]
 * @param seqData       the sequence data obtained [out]
 * @param seqData       the resulting data [out]
 * @param queryData     the query sequence [in]
 * @param queryOffset   offset for align if there are multiple queries
 * @param align          information about the alignment between query and subject
 * @param shouldTestIdentical did alignment pass a preliminary test in
 *                       redo_alignment.c that indicates the sequence
 *                        pieces may be near identical
 *
 * @return 0 on success; -1 on failure
 */
static int
s_SequenceGetRange(const BlastCompo_MatchingSequence * self,
                   const BlastCompo_SequenceRange * range,
                   BlastCompo_SequenceData * seqData,
		   const BlastCompo_SequenceData * queryData,
		   const int queryOffset,
		   const BlastCompo_Alignment *align,
		   const Boolean shouldTestIdentical)
{
    BlastKappa_SequenceInfo * seq_info = self->local_data;
    if (seq_info->prog_number ==  eBlastTypeTblastn) {
        /* The sequence must be translated. */
      return s_SequenceGetTranslatedRange(self, range, seqData,
					  queryData, queryOffset, align, shouldTestIdentical);
    } else {
      return s_SequenceGetProteinRange(self, range, seqData, queryData, queryOffset, align, shouldTestIdentical);
    }
}


/** Data and data-structures needed to perform a gapped alignment */
typedef struct BlastKappa_GappingParamsContext {
    const BlastScoringParameters*
        scoringParams;                /**< scoring parameters for a
                                           gapped alignment */
    BlastGapAlignStruct * gap_align;  /**< additional parameters for a
                                           gapped alignment */
    BlastScoreBlk* sbp;               /**< the score block for this search */
    double localScalingFactor;        /**< the amount by which this
                                           search has been scaled */
    EBlastProgramType prog_number;    /**< the type of search being
                                           performed */
} BlastKappa_GappingParamsContext;


/**
 * Reads a BlastGapAlignStruct that has been used to compute a
 * traceback, and return a BlastCompo_Alignment representing the
 * alignment.  The BlastGapAlignStruct is in coordinates local to the
 * ranges being aligned; the resulting alignment is in coordinates w.r.t.
 * the whole query and subject.
 *
 * @param gap_align         the BlastGapAlignStruct
 * @param *edit_script      the edit script from the alignment; on exit
 *                          NULL.  The edit_script is usually
 *                          gap_align->edit_script, but we don't want
 *                          an implicit side effect on the gap_align.
 * @param query_range       the range of the query used in this alignment
 * @param subject_range     the range of the subject used in this alignment
 * @param matrix_adjust_rule   the rule used to compute the scoring matrix
 *
 * @return the new alignment on success or NULL on error
 */
static BlastCompo_Alignment *
s_NewAlignmentFromGapAlign(BlastGapAlignStruct * gap_align,
                           GapEditScript ** edit_script,
                           BlastCompo_SequenceRange * query_range,
                           BlastCompo_SequenceRange * subject_range,
                           EMatrixAdjustRule matrix_adjust_rule)
{
    /* parameters to BlastCompo_AlignmentNew */
    int queryStart, queryEnd, queryIndex, matchStart, matchEnd, frame;
    BlastCompo_Alignment * obj; /* the new alignment */

    /* In the composition_adjustment library, the query start/end are
       indices into the concatenated query, and so must be shifted.  */
    queryStart = gap_align->query_start + query_range->begin;
    queryEnd   = gap_align->query_stop + query_range->begin;
    queryIndex = query_range->context;
    matchStart = gap_align->subject_start + subject_range->begin;
    matchEnd   = gap_align->subject_stop  + subject_range->begin;
    frame      = subject_range->context;

    obj = BlastCompo_AlignmentNew(gap_align->score, matrix_adjust_rule,
                                  queryStart, queryEnd, queryIndex,
                                  matchStart, matchEnd, frame,
                                  *edit_script);
    if (obj != NULL) {
        *edit_script = NULL;
    }
    return obj;
}


/** A callback used when performing SmithWaterman alignments:
 * Calculate the traceback for one alignment by performing an x-drop
 * alignment in the forward direction, possibly increasing the x-drop
 * parameter until the desired score is attained.
 *
 * The start, end and score of the alignment should be obtained
 * using the Smith-Waterman algorithm before this routine is called.
 *
 * @param *pnewAlign       the new alignment
 * @param *pqueryEnd       on entry, the end of the alignment in the
 *                         query, as computed by the Smith-Waterman
 *                         algorithm.  On exit, the end as computed by
 *                         the x-drop algorithm
 * @param *pmatchEnd       like as *pqueryEnd, but for the subject
 *                         sequence
 * @param queryStart       the starting point in the query
 * @param matchStart       the starting point in the subject
 * @param score            the score of the alignment, as computed by
 *                         the Smith-Waterman algorithm
 * @param query            query sequence data
 * @param query_range      range of this query in the concatenated
 *                         query
 * @param ccat_query_length   total length of the concatenated query
 * @param subject          subject sequence data
 * @param subject_range    range of subject_data in the translated
 *                         query, in amino acid coordinates
 * @param full_subject_length   length of the full subject sequence
 * @param gapping_params        parameters used to compute gapped
 *                              alignments
 * @param matrix_adjust_rule    the rule used to compute the scoring matrix
 *
 * @returns 0   (posts a fatal error if it fails)
 * @sa new_xdrop_align_type
 */
static int
s_NewAlignmentUsingXdrop(BlastCompo_Alignment ** pnewAlign,
                         Int4 * pqueryEnd, Int4 *pmatchEnd,
                         Int4 queryStart, Int4 matchStart, Int4 score,
                         BlastCompo_SequenceData * query,
                         BlastCompo_SequenceRange * query_range,
                         Int4 ccat_query_length,
                         BlastCompo_SequenceData * subject,
                         BlastCompo_SequenceRange * subject_range,
                         Int4 full_subject_length,
                         BlastCompo_GappingParams * gapping_params,
                         EMatrixAdjustRule matrix_adjust_rule)
{
    Int4 newScore;
    /* Extent of the alignment as computed by an x-drop alignment
     * (usually the same as (queryEnd - queryStart) and (matchEnd -
     * matchStart)) */
    Int4 queryExtent, matchExtent;
    BlastCompo_Alignment * obj = NULL;  /* the new object */
    /* BLAST-specific parameters needed compute an X-drop alignment */
    BlastKappa_GappingParamsContext * context = gapping_params->context;
    /* Auxiliarly structure for computing gapped alignments */
    BlastGapAlignStruct * gap_align = context->gap_align;
    /* Scoring parameters for gapped alignments */
    const BlastScoringParameters* scoringParams = context->scoringParams;
    /* A structure containing the traceback of a gapped alignment */
    GapEditScript* editScript = NULL;

    /* suppress unused parameter warnings; this is a callback
       function, so these parameter cannot be deleted */
    (void) ccat_query_length;
    (void) full_subject_length;

    gap_align->gap_x_dropoff = gapping_params->x_dropoff;

    s_SWFindFinalEndsUsingXdrop(query,   queryStart, *pqueryEnd,
                                subject, matchStart, *pmatchEnd,
                                gap_align, scoringParams,
                                score, &queryExtent, &matchExtent,
                                &newScore);
    *pqueryEnd = queryStart + queryExtent;
    *pmatchEnd = matchStart + matchExtent;

    editScript =
        Blast_PrelimEditBlockToGapEditScript(gap_align->rev_prelim_tback,
                                             gap_align->fwd_prelim_tback);
    if (editScript != NULL) {
        /* Shifted values of the endpoints */
        Int4 aqueryStart =  queryStart + query_range->begin;
        Int4 aqueryEnd   = *pqueryEnd  + query_range->begin;
        Int4 amatchStart =  matchStart + subject_range->begin;
        Int4 amatchEnd   = *pmatchEnd  + subject_range->begin;

        obj = BlastCompo_AlignmentNew(newScore, matrix_adjust_rule,
                                      aqueryStart, aqueryEnd,
                                      query_range->context,
                                      amatchStart, amatchEnd,
                                      subject_range->context, editScript);
        if (obj == NULL) {
            GapEditScriptDelete(editScript);
        }
    }
    *pnewAlign = obj;

    return obj != NULL ? 0 : -1;
}


/**
 * A callback: calculate the traceback for one alignment by
 * performing an x-drop alignment in both directions
 *
 * @param in_align         the existing alignment, without traceback
 * @param matrix_adjust_rule    the rule used to compute the scoring matrix
 * @param query_data       query sequence data
 * @param query_range      range of this query in the concatenated
 *                         query
 * @param ccat_query_length   total length of the concatenated query
 * @param subject_data     subject sequence data
 * @param subject_range    range of subject_data in the translated
 *                         query, in amino acid coordinates
 * @param full_subject_length   length of the full subject sequence
 * @param gapping_params        parameters used to compute gapped
 *                              alignments
 * @sa redo_one_alignment_type
 */
static BlastCompo_Alignment *
s_RedoOneAlignment(BlastCompo_Alignment * in_align,
                   EMatrixAdjustRule matrix_adjust_rule,
                   BlastCompo_SequenceData * query_data,
                   BlastCompo_SequenceRange * query_range,
                   int ccat_query_length,
                   BlastCompo_SequenceData * subject_data,
                   BlastCompo_SequenceRange * subject_range,
                   int full_subject_length,
                   BlastCompo_GappingParams * gapping_params)
{
    int status;                /* return code */
    Int4 q_start, s_start;     /* starting point in query and subject */
    /* BLAST-specific parameters needed to compute a gapped alignment */
    BlastKappa_GappingParamsContext * context = gapping_params->context;
    /* Score block for this search */
    BlastScoreBlk* sbp = context->sbp;
    /* Auxiliary structure for computing gapped alignments */
    BlastGapAlignStruct* gapAlign = context->gap_align;
    /* The preliminary gapped HSP that were are recomputing */
    BlastHSP * hsp = in_align->context;

    /* suppress unused parameter warnings; this is a callback
       function, so these parameter cannot be deleted */
    (void) ccat_query_length;
    (void) full_subject_length;

    /* Shift the subject offset and gapped start to be offsets
       into the translated subject_range; shifting in this manner
       is necessary for BLAST_CheckStartForGappedAlignment */
    hsp->subject.offset       -= subject_range->begin;
    hsp->subject.end          -= subject_range->begin;
    hsp->subject.gapped_start -= subject_range->begin;

    if(BLAST_CheckStartForGappedAlignment(hsp, query_data->data,
                                          subject_data->data, sbp)) {
        /* We may use the starting point supplied by the HSP. */
        q_start = hsp->query.gapped_start;
        s_start = hsp->subject.gapped_start;
    } else {
        /* We must recompute the start for the gapped alignment, as the
           one in the HSP was unacceptable.*/
        Boolean retval =
            BlastGetOffsetsForGappedAlignment(query_data->data,
                                            subject_data->data, sbp,
                                            hsp,
                                            &q_start, 
                                            &s_start);
        /* ASSERT(retval == TRUE); */
        if (retval == FALSE)
           return NULL;
    }
    /* Undo the shift so there is no side effect on the incoming HSP
       list. */
    hsp->subject.offset       += subject_range->begin;
    hsp->subject.end          += subject_range->begin;
    hsp->subject.gapped_start += subject_range->begin;

    gapAlign->gap_x_dropoff = gapping_params->x_dropoff;

    status =
        BLAST_GappedAlignmentWithTraceback(context->prog_number,
                                           query_data->data,
                                           subject_data->data, gapAlign,
                                           context->scoringParams,
                                           q_start, s_start,
                                           query_data->length,
                                           subject_data->length,
                                           NULL);
    if (status == 0) {
        return s_NewAlignmentFromGapAlign(gapAlign, &gapAlign->edit_script,
                                          query_range, subject_range,
                                          matrix_adjust_rule);
    } else {
        return NULL;
    }
}


/**
 * A BlastKappa_SavedParameters holds the value of certain search
 * parameters on entry to RedoAlignmentCore.  These values are
 * restored on exit.
 */
typedef struct BlastKappa_SavedParameters {
    Int4          gap_open;    /**< a penalty for the existence of a gap */
    Int4          gapExtend;   /**< a penalty for each residue in the
                                    gap */
    double        scale_factor;     /**< the original scale factor */
    Int4 **origMatrix;              /**< The original matrix values */
    double original_expect_value;   /**< expect value on entry */
    /** copy of the original gapped Karlin-Altschul block
     * corresponding to the first context */
    Blast_KarlinBlk** kbp_gap_orig;
    Int4             num_queries;   /**< Number of queries in this search */
} BlastKappa_SavedParameters;


/**
 * Release the data associated with a BlastKappa_SavedParameters and
 * delete the object
 * @param searchParams the object to be deleted [in][out]
 */
static void
s_SavedParametersFree(BlastKappa_SavedParameters ** searchParams)
{
    /* for convenience, remove one level of indirection from searchParams */
    BlastKappa_SavedParameters *sp = *searchParams;

    if (sp != NULL) {
        if (sp->kbp_gap_orig != NULL) {
            int i;
            for (i = 0;  i < sp->num_queries;  i++) {
                if (sp->kbp_gap_orig[i] != NULL)
                    Blast_KarlinBlkFree(sp->kbp_gap_orig[i]);
            }
            free(sp->kbp_gap_orig);
        }
        if (sp->origMatrix != NULL)
            Nlm_Int4MatrixFree(&sp->origMatrix);
    }
    sfree(*searchParams);
    *searchParams = NULL;
}


/**
 * Create a new instance of BlastKappa_SavedParameters
 *
 * @param rows               number of rows in the scoring matrix
 * @param numQueries         number of queries in this search
 * @param compo_adjust_mode  if >0, use composition-based statistics
 * @param positionBased      if true, the search is position-based
 */
static BlastKappa_SavedParameters *
s_SavedParametersNew(Int4 rows,
                     Int4 numQueries,
                     ECompoAdjustModes compo_adjust_mode,
                     Boolean positionBased)
{
    int i;
    BlastKappa_SavedParameters *sp;   /* the new object */
    sp = malloc(sizeof(BlastKappa_SavedParameters));

    if (sp == NULL) {
        goto error_return;
    }
    sp->kbp_gap_orig       = NULL;
    sp->origMatrix         = NULL;

    sp->kbp_gap_orig = calloc(numQueries, sizeof(Blast_KarlinBlk*));
    if (sp->kbp_gap_orig == NULL) {
        goto error_return;
    }
    sp->num_queries = numQueries;
    for (i = 0;  i < numQueries;  i++) {
        sp->kbp_gap_orig[i] = NULL;
    }
    if (compo_adjust_mode != eNoCompositionBasedStats) {
        if (positionBased) {
            sp->origMatrix = Nlm_Int4MatrixNew(rows, BLASTAA_SIZE);
        } else {
            sp->origMatrix = Nlm_Int4MatrixNew(BLASTAA_SIZE, BLASTAA_SIZE);
        }
        if (sp->origMatrix == NULL)
            goto error_return;
    }
    return sp;
error_return:
    s_SavedParametersFree(&sp);
    return NULL;
}


/**
 * Record the initial value of the search parameters that are to be
 * adjusted.
 *
 * @param searchParams       holds the recorded values [out]
 * @param sbp                a score block [in]
 * @param scoring            gapped alignment parameters [in]
 * @param query_length       length of the concatenated query [in]
 * @param compo_adjust_mode  composition adjustment mode [in]
 * @param positionBased     is this search position-based [in]
 */
static int
s_RecordInitialSearch(BlastKappa_SavedParameters * searchParams,
                      BlastScoreBlk* sbp,
                      const BlastScoringParameters* scoring,
                      int query_length,
                      ECompoAdjustModes compo_adjust_mode,
                      Boolean positionBased)
{
    int i;

    searchParams->gap_open     = scoring->gap_open;
    searchParams->gapExtend    = scoring->gap_extend;
    searchParams->scale_factor = scoring->scale_factor;

    for (i = 0;  i < searchParams->num_queries;  i++) { 
        if (sbp->kbp_gap[i] != NULL) {
            /* There is a kbp_gap for query i and it must be copied */
            searchParams->kbp_gap_orig[i] = Blast_KarlinBlkNew();
            if (searchParams->kbp_gap_orig[i] == NULL) {
                return -1;
            }
            Blast_KarlinBlkCopy(searchParams->kbp_gap_orig[i],
                                sbp->kbp_gap[i]);
        }
    }

    if (compo_adjust_mode != eNoCompositionBasedStats) {
        Int4 **matrix;              /* scoring matrix */
        int j;                      /* iteration index */
        int rows;                   /* number of rows in matrix */
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
    return 0;
}


/**
 * Rescale the search parameters in the search object and options
 * object to obtain more precision.
 *
 * @param sbp               score block to be rescaled
 * @param sp                scoring parameters to be rescaled
 * @param num_queries       number of queries in this search
 * @param scale_factor      amount by which to scale this search
 */
static void
s_RescaleSearch(BlastScoreBlk* sbp,
                BlastScoringParameters* sp,
                int num_queries,
                double scale_factor)
{
    int i;
    for (i = 0;  i < num_queries;  i++) {
        if (sbp->kbp_gap[i] != NULL) {
            Blast_KarlinBlk * kbp = sbp->kbp_gap[i];
            kbp->Lambda /= scale_factor;
            kbp->logK = log(kbp->K);
        }
    }

    sp->gap_open = BLAST_Nint(sp->gap_open  * scale_factor);
    sp->gap_extend = BLAST_Nint(sp->gap_extend * scale_factor);
    sp->scale_factor = scale_factor;
}


/**
 * Restore the parameters that were adjusted to their original values.
 *
 * @param sbp                the score block to be restored
 * @param scoring            the scoring parameters to be restored
 * @param searchParams       the initial recorded values of the parameters
 * @param query_length       the concatenated query length
 * @param positionBased      is this search position-based
 * @param compo_adjust_mode  mode of composition adjustment
 */
static void
s_RestoreSearch(BlastScoreBlk* sbp,
                BlastScoringParameters* scoring,
                const BlastKappa_SavedParameters * searchParams,
                int query_length,
                Boolean positionBased,
                ECompoAdjustModes compo_adjust_mode)
{
    int i;

    scoring->gap_open = searchParams->gap_open;
    scoring->gap_extend = searchParams->gapExtend;
    scoring->scale_factor = searchParams->scale_factor;

    for (i = 0;  i < searchParams->num_queries;  i++) {
        if (sbp->kbp_gap[i] != NULL) {
            Blast_KarlinBlkCopy(sbp->kbp_gap[i],
                                searchParams->kbp_gap_orig[i]);
        }
    }
    if(compo_adjust_mode != eNoCompositionBasedStats) {
        int  j;             /* iteration index */
        Int4 ** matrix;     /* matrix to be restored */
        int rows;           /* number of rows in the matrix */

        if (positionBased) {
            matrix = sbp->psi_matrix->pssm->data;
            rows = query_length;
        } else {
            matrix = sbp->matrix->data;
            rows = BLASTAA_SIZE;
        }
        for (i = 0;  i < rows;  i++) {
            for (j = 0;  j < BLASTAA_SIZE;  j++) {
                matrix[i][j] = searchParams->origMatrix[i][j];
            }
        }
    }
}


/**
 * Initialize an object of type Blast_MatrixInfo.
 *
 * @param self            object being initialized
 * @param queryBlk        the query sequence data
 * @param sbp             score block for this search
 * @param scale_factor    amount by which ungapped parameters should be
 *                        scaled
 * @param matrixName      name of the matrix
 */
static int
s_MatrixInfoInit(Blast_MatrixInfo * self,
                 BLAST_SequenceBlk* queryBlk,
                 BlastScoreBlk* sbp,
                 double scale_factor,
                 const char * matrixName)
{
    int status = 0;    /* return status */
    int lenName;       /* length of matrixName as a string */

    /* copy the matrix name (strdup is not standard C) */
    lenName = strlen(matrixName);
    if (NULL == (self->matrixName = malloc(lenName + 1))) {
        return -1;
    }
    memcpy(self->matrixName, matrixName, lenName + 1);

    if (self->positionBased) {
        status = s_GetPosBasedStartFreqRatios(self->startFreqRatios,
                                              queryBlk->length,
                                              queryBlk->sequence,
                                              matrixName,
                                              sbp->psi_matrix->freq_ratios);
        if (status == 0) {
            status = s_ScalePosMatrix(self->startMatrix, matrixName,
                                      sbp->psi_matrix->freq_ratios,
                                      queryBlk->sequence,
                                      queryBlk->length, sbp, scale_factor);
            self->ungappedLambda = sbp->kbp_psi[0]->Lambda / scale_factor;
        }
    } else {
        self->ungappedLambda = sbp->kbp_ideal->Lambda / scale_factor;
        status = s_GetStartFreqRatios(self->startFreqRatios, matrixName);
        if (status == 0) {
            Blast_Int4MatrixFromFreq(self->startMatrix, self->cols,
                                     self->startFreqRatios,
                                     self->ungappedLambda);
        }
    }
    return status;
}


/**
 * Save information about all queries in an array of objects of type
 * BlastCompo_QueryInfo.
 *
 * @param query_data        query sequence data
 * @param blast_query_info  information about all queries, as an
 *                          internal blast data structure
 *
 * @return the new array on success, or NULL on error
 */
static BlastCompo_QueryInfo *
s_GetQueryInfo(Uint1 * query_data, BlastQueryInfo * blast_query_info)
{
    int i;                   /* loop index */
    BlastCompo_QueryInfo *
        compo_query_info;    /* the new array */
    int num_queries;         /* the number of queries/elements in
                                compo_query_info */

    num_queries = blast_query_info->num_queries;
    compo_query_info = calloc(num_queries, sizeof(BlastCompo_QueryInfo));
    if (compo_query_info != NULL) {
        for (i = 0;  i < num_queries;  i++) {
            BlastCompo_QueryInfo * query_info = &compo_query_info[i];
            BlastContextInfo * query_context = &blast_query_info->contexts[i];

            query_info->eff_search_space =
                (double) query_context->eff_searchsp;
            query_info->origin = query_context->query_offset;
            query_info->seq.data = &query_data[query_info->origin];
            query_info->seq.length = query_context->query_length;

            Blast_ReadAaComposition(&query_info->composition, BLASTAA_SIZE,
                                    query_info->seq.data,
                                    query_info->seq.length);
        }
    }
    return compo_query_info;
}


/**
 * Create a new object of type BlastCompo_GappingParams.  The new
 * object contains the parameters needed by the composition adjustment
 * library to compute a gapped alignment.
 *
 * @param context     the data structures needed by callback functions
 *                    that perform the gapped alignments.
 * @param extendParams parameters used for a gapped extension
 * @param num_queries  the number of queries in the concatenated query
 */
static BlastCompo_GappingParams *
s_GappingParamsNew(BlastKappa_GappingParamsContext * context,
                   const BlastExtensionParameters* extendParams,
                   int num_queries)
{
    int i;
    double min_lambda = DBL_MAX;   /* smallest gapped Lambda */
    const BlastScoringParameters * scoring = context->scoringParams;
    const BlastExtensionOptions * options = extendParams->options;
    /* The new object */
    BlastCompo_GappingParams * gapping_params = NULL;

    gapping_params = malloc(sizeof(BlastCompo_GappingParams));
    if (gapping_params != NULL) {
        gapping_params->gap_open = scoring->gap_open;
        gapping_params->gap_extend = scoring->gap_extend;
        gapping_params->context = context;
    }
    
    for (i = 0;  i < num_queries;  i++) {
        if (context->sbp->kbp_gap[i] != NULL &&
            context->sbp->kbp_gap[i]->Lambda < min_lambda) {
            min_lambda = context->sbp->kbp_gap[i]->Lambda;
        }
    }
    gapping_params->x_dropoff = (Int4)
        MAX(options->gap_x_dropoff_final*NCBIMATH_LN2 / min_lambda,
            extendParams->gap_x_dropoff_final);
    context->gap_align->gap_x_dropoff = gapping_params->x_dropoff;

    return gapping_params;
}


/** Callbacks used by the Blast_RedoOneMatch* routines */
static const Blast_RedoAlignCallbacks
redo_align_callbacks = {
    s_CalcLambda, s_SequenceGetRange, s_RedoOneAlignment,
    s_NewAlignmentUsingXdrop, s_FreeEditScript
};


/** 
 * Read the parameters required for the Blast_RedoOneMatch* functions from
 * the corresponding parameters in standard BLAST datatypes.  Return a new
 * object representing these parameters.
 */
static Blast_RedoAlignParams *
s_GetAlignParams(BlastKappa_GappingParamsContext * context,
                 BLAST_SequenceBlk * queryBlk,
                 BlastQueryInfo* queryInfo,
                 const BlastHitSavingParameters* hitParams,
                 const BlastExtensionParameters* extendParams)
{
    int status = 0;    /* status code */
    int rows;          /* number of rows in the scoring matrix */
    int cutoff_s;      /* cutoff score for saving an alignment */
    double cutoff_e;   /* cutoff evalue for saving an alignment */
    BlastCompo_GappingParams *
        gapping_params = NULL;    /* parameters needed to compute a gapped
                                     alignment */
    Blast_MatrixInfo *
        scaledMatrixInfo;         /* information about the scoring matrix */
    /* does this kind of search translate the database sequence */
    int subject_is_translated = context->prog_number == eBlastTypeTblastn;
    /* is this a positiion-based search */
    Boolean positionBased = (Boolean) (context->sbp->psi_matrix != NULL);
    /* will BLAST_LinkHsps be called to assign e-values */
    Boolean do_link_hsps = (hitParams->do_sum_stats);
    ECompoAdjustModes compo_adjust_mode =
        (ECompoAdjustModes) extendParams->options->compositionBasedStats;
    
    if (do_link_hsps) {
        ASSERT(hitParams->link_hsp_params != NULL);
        cutoff_s =
            (int) (hitParams->cutoff_score_min * context->localScalingFactor);
    } else {
        /* There is no cutoff score; we consider e-values instead */
        cutoff_s = 1;
    }
    cutoff_e = hitParams->options->expect_value;
    rows = positionBased ? queryInfo->max_length : BLASTAA_SIZE;
    scaledMatrixInfo = Blast_MatrixInfoNew(rows, BLASTAA_SIZE, positionBased);
    status = s_MatrixInfoInit(scaledMatrixInfo, queryBlk, context->sbp,
                              context->localScalingFactor,
                              context->scoringParams->options->matrix);
    if (status != 0) {
        return NULL;
    }
    gapping_params = s_GappingParamsNew(context, extendParams,
                                        queryInfo->num_queries);
    if (gapping_params == NULL) {
        return NULL;
    } else {
        return
            Blast_RedoAlignParamsNew(&scaledMatrixInfo, &gapping_params,
                                     compo_adjust_mode, positionBased,
                                     subject_is_translated,
                                     queryInfo->max_length, cutoff_s, cutoff_e,
                                     do_link_hsps, &redo_align_callbacks);
    }
}


/**
 * Convert an array of BlastCompo_Heap objects to a BlastHSPResults structure.
 *
 * @param results        BLAST core external results structure (pre-SeqAlign)
 *                       [out]
 * @param heaps          an array of BlastCompo_Heap objects
 * @param hitlist_size   size of each list in the results structure above [in]
 */
static void
s_FillResultsFromCompoHeaps(BlastHSPResults * results,
                            BlastCompo_Heap heaps[],
                            Int4 hitlist_size)
{
    int query_index;   /* loop index */
    int num_queries;   /* Number of queries in this search */

    num_queries = results->num_queries; 
    for (query_index = 0;  query_index < num_queries;  query_index++) {
        BlastHSPList* hsp_list;
        BlastHitList* hitlist;
        BlastCompo_Heap * heap = &heaps[query_index];

        results->hitlist_array[query_index] = Blast_HitListNew(hitlist_size);
        hitlist = results->hitlist_array[query_index];

        while (NULL != (hsp_list = BlastCompo_HeapPop(heap))) {
            Blast_HitListUpdate(hitlist, hsp_list);
        }
    }
    Blast_HSPResultsReverseOrder(results);
}


/** Remove all matches from a BlastCompo_Heap. */
static void s_ClearHeap(BlastCompo_Heap * self)
{
    BlastHSPList* hsp_list = NULL;   /* an element of the heap */

    while (NULL != (hsp_list = BlastCompo_HeapPop(self))) {
        hsp_list = Blast_HSPListFree(hsp_list);
    }
}


/**
 *  Recompute alignments for each match found by the gapped BLAST
 *  algorithm.
 */
Int2
Blast_RedoAlignmentCore(EBlastProgramType program_number,
                        BLAST_SequenceBlk * queryBlk,
                        BlastQueryInfo* queryInfo,
                        BlastScoreBlk* sbp,
                        BlastHSPStream* hsp_stream,
                        const BlastSeqSrc* seqSrc,
                        Int4 default_db_genetic_code,
                        BlastScoringParameters* scoringParams,
                        const BlastExtensionParameters* extendParams,
                        const BlastHitSavingParameters* hitParams,
                        const PSIBlastOptions* psiOptions,
                        BlastHSPResults* results)
{
    int status_code = 0;                    /* return value code */
    /* the factor by which to scale the scoring system in order to
     * obtain greater precision */
    double localScalingFactor;
    /* the values of the search parameters that will be recorded, altered
     * in the search structure in this routine, and then restored before
     * the routine exits. */
    BlastKappa_SavedParameters *savedParams = NULL;
    /* forbidden ranges for each database position (used in
     * Smith-Waterman alignments) */
    Blast_ForbiddenRanges forbidden = {0,};
    /* a collection of alignments for each query sequence with
     * sequences from the database */
    BlastCompo_Heap * redoneMatches = NULL;
    /* stores all fields needed for computing a compositionally
     * adjusted score matrix using Newton's method */
    Blast_CompositionWorkspace *NRrecord = NULL;
    /* loop index */
    int query_index;
    /* number of queries in the concatenated query */
    int numQueries = queryInfo->num_queries;
    /* keeps track of gapped alignment params */
    BlastGapAlignStruct* gapAlign = NULL;
    /* All alignments above this value will be reported, no matter how
     * many. */
    double inclusion_ethresh;
    /* array of lists of alignments for each query to this subject */
    BlastCompo_Alignment ** alignments = NULL;

    BlastCompo_QueryInfo * query_info = NULL;
    Blast_RedoAlignParams * redo_align_params = NULL;
    Boolean positionBased = (Boolean) (sbp->psi_matrix != NULL);
    ECompoAdjustModes compo_adjust_mode =
        (ECompoAdjustModes) extendParams->options->compositionBasedStats;
    Boolean smithWaterman =
        (Boolean) (extendParams->options->eTbackExt == eSmithWatermanTbck);
    /* alignment data for the current query-subject match */
    BlastHSPList* thisMatch = NULL;
    /* existing alignments for a match */
    BlastCompo_Alignment * incoming_aligns = NULL;
    Int4      **matrix;                   /* score matrix */
    BlastKappa_GappingParamsContext gapping_params_context;

    double pvalueForThisPair = (-1); /* p-value for this match
                                        for composition; -1 == no adjustment*/
    double LambdaRatio; /*lambda ratio*/
    /* which test function do we use to see if a composition-adjusted
       p-value is desired; value needs to be passed in eventually*/
    int compositionTestIndex = extendParams->options->unifiedP;
    Uint1* genetic_code_string = GenCodeSingletonFind(default_db_genetic_code);
    Boolean perform_partial_fetch = BlastSeqSrcGetSupportsPartialFetching(seqSrc);

    ASSERT(program_number == eBlastTypeBlastp ||
           program_number == eBlastTypeTblastn ||
           program_number == eBlastTypePsiBlast);

    if (positionBased) {
        matrix = sbp->psi_matrix->pssm->data;
    } else {
        matrix = sbp->matrix->data;
    }
    /**** Validate parameters *************/
    if (matrix == NULL) {
        return -1;
    }
    if (0 == strcmp(scoringParams->options->matrix, "BLOSUM62_20") &&
        compo_adjust_mode == eNoCompositionBasedStats) {
        return -1;                   /* BLOSUM62_20 only makes sense if
                                      * compo_adjust_mode is on */
    }
    if (positionBased) {
        /* Position based searches can only use traditional
         * composition based stats */
        if ((int) compo_adjust_mode > 1) {
            compo_adjust_mode = eCompositionBasedStats;
        }
        /* A position-based search can only have one query */
        ASSERT(queryInfo->num_queries == 1);
        ASSERT(queryBlk->length == (Int4)sbp->psi_matrix->pssm->ncols);
    }
    if ((int) compo_adjust_mode > 1 &&
        !Blast_FrequencyDataIsAvailable(scoringParams->options->matrix)) {
        return -1;   /* Unsupported matrix */
    }
    /*****************/
    inclusion_ethresh = (psiOptions /* this can be NULL for CBl2Seq */
                         ? psiOptions->inclusion_ethresh 
                         : PSI_INCLUSION_ETHRESH);
    ASSERT(inclusion_ethresh != 0.0);

    /* Initialize savedParams */
    savedParams =
        s_SavedParametersNew(queryInfo->max_length, queryInfo->num_queries,
                             compo_adjust_mode, positionBased);
    if (savedParams == NULL) {
        status_code = -1;
        goto function_cleanup;
    }
    status_code =
        s_RecordInitialSearch(savedParams, sbp, scoringParams,
                              queryInfo->max_length, compo_adjust_mode,
                              positionBased);
    if (status_code != 0) {
        goto function_cleanup;
    }
    if (compo_adjust_mode != eNoCompositionBasedStats) {
        if((0 == strcmp(scoringParams->options->matrix, "BLOSUM62_20"))) {
            localScalingFactor = SCALING_FACTOR / 10;
        } else {
            localScalingFactor = SCALING_FACTOR;
        }
    } else {
        localScalingFactor = 1.0;
    }
    s_RescaleSearch(sbp, scoringParams, queryInfo->num_queries,
                    localScalingFactor);
    status_code =
        BLAST_GapAlignStructNew(scoringParams, extendParams,
                                BlastSeqSrcGetMaxSeqLen(seqSrc), sbp,
                                &gapAlign);
    if (status_code != 0) {
        return (Int2) status_code;
    }
    gapping_params_context.gap_align = gapAlign;
    gapping_params_context.scoringParams = scoringParams;
    gapping_params_context.sbp = sbp;
    gapping_params_context.localScalingFactor = localScalingFactor;
    gapping_params_context.prog_number = program_number;
    redo_align_params =
        s_GetAlignParams(&gapping_params_context, queryBlk, queryInfo, 
                         hitParams, extendParams);
    if (redo_align_params == NULL) {
        status_code = -1;
        goto function_cleanup;
    }
    query_info = s_GetQueryInfo(queryBlk->sequence, queryInfo);
    if (query_info == NULL) {
        status_code = -1;
        goto function_cleanup;
    }
    if(smithWaterman) {
        status_code =
            Blast_ForbiddenRangesInitialize(&forbidden, queryInfo->max_length);
        if (status_code != 0) {
            goto function_cleanup;
        }
    }
    redoneMatches = calloc(numQueries, sizeof(BlastCompo_Heap));
    if (redoneMatches == NULL) {
        status_code = -1;
        goto function_cleanup;
    }
    for (query_index = 0;  query_index < numQueries;  query_index++) {
        status_code =
            BlastCompo_HeapInitialize(&redoneMatches[query_index],
                                      hitParams->options->hitlist_size,
                                      inclusion_ethresh);
        if (status_code != 0) {
            goto function_cleanup;
        }
    }
    if( (int) compo_adjust_mode > 1 && !positionBased ) {
        NRrecord = Blast_CompositionWorkspaceNew();
        status_code =
            Blast_CompositionWorkspaceInit(NRrecord,
                                           scoringParams->options->matrix);
        if (status_code != 0) {
            goto function_cleanup;
        }
    }
    alignments = calloc(numQueries, sizeof(BlastCompo_Alignment *));
    if (alignments == NULL) {
        status_code = -1;
        goto function_cleanup;
    }
    while (BlastHSPStreamRead(hsp_stream, &thisMatch) != kBlastHSPStream_Eof) {
        /* for all matching sequences */
        Blast_KarlinBlk * kbp;

        /* the data for a matching database sequence */
        BlastCompo_MatchingSequence matchingSeq = {0,};
        if(thisMatch->hsp_array == NULL) {
            continue;
        }
        if (BlastCompo_EarlyTermination(thisMatch->best_evalue,
                                        redoneMatches, numQueries)) {
            break;
        }
        /* Get the sequence for this match */
        if (perform_partial_fetch) {
            
            Int4 oid = thisMatch->oid;
            Int4 i;
            BlastHSPRangeList *range_list = NULL;

            ASSERT(Blast_SubjectIsTranslated(program_number));
            for (i=0; i<thisMatch->hspcnt; i++) {
                BlastHSP *hsp = thisMatch->hsp_array[i];
                Int4 begin = (hsp->subject.offset - 2) * CODON_LENGTH;
                Int4 end = (hsp->subject.end + 2) * CODON_LENGTH;
                if (hsp->subject.frame < 0) {
                    Int4 len = BlastSeqSrcGetSeqLen(seqSrc, &oid);
                    Int4 begin_new = len - end;
                    end = len - begin;
                    begin = begin_new;
                }
                range_list = BlastHSPRangeListAddRange(range_list, begin, end);
            }
            {{
            BlastSeqSrcSetRangesArg *arg = BlastSeqSrcSetRangesArgNew(thisMatch->hspcnt);
            arg->oid = oid;
            
            BlastHSPRangeBuildSetRangesArg(range_list, arg);
            BlastSeqSrcSetSeqRanges(seqSrc, arg);
            BlastHSPRangeListFree(range_list);
            BlastSeqSrcSetRangesArgFree(arg);
            }}
        }

        status_code =
            s_MatchingSequenceInitialize(&matchingSeq, program_number,
                                         seqSrc, default_db_genetic_code,
                                         thisMatch->oid);
        if (status_code != 0) {
            goto match_loop_cleanup;
        }
        incoming_aligns =
            s_ResultHspToDistinctAlign(thisMatch->hsp_array, thisMatch->hspcnt,
                                       queryInfo, localScalingFactor);
        if (incoming_aligns == NULL) {
            status_code = -1;
            goto match_loop_cleanup;
        }
        /* All alignments in thisMatch should be to the same query */
        kbp = sbp->kbp_gap[thisMatch->query_index];
        if (smithWaterman) {
            status_code =
                Blast_RedoOneMatchSmithWaterman(alignments,
                                                redo_align_params,
                                                incoming_aligns,
                                                thisMatch->hspcnt,
                                                kbp->Lambda, kbp->logK,
                                                &matchingSeq, query_info,
                                                numQueries,
                                                matrix, BLASTAA_SIZE,
                                                NRrecord, &forbidden,
                                                redoneMatches,
                                                &pvalueForThisPair,
                                                compositionTestIndex,
                                                &LambdaRatio);
        } else {
            status_code =
                Blast_RedoOneMatch(alignments, redo_align_params,
                                   incoming_aligns, thisMatch->hspcnt,
                                   kbp->Lambda, &matchingSeq,
                                   queryInfo->max_length, query_info,
                                   numQueries, matrix, BLASTAA_SIZE,
                                   NRrecord, &pvalueForThisPair,
                                   compositionTestIndex,
                                   &LambdaRatio);
        }
        if (status_code != 0) {
            goto match_loop_cleanup;
        }
        for (query_index = 0;  query_index < numQueries;  query_index++) {
            /* Loop over queries */
            if( alignments[query_index] != NULL) { /* alignments were found */
                double bestEvalue;   /* best e-value among alignments in the
                                        hitlist */
                Int4 bestScore;      /* best score among alignments in
                                        the hitlist */
                /* a hitlist containing the newly-computed alignments */
                BlastHSPList * hsp_list = NULL;
                void * discardedAligns = NULL;
                hsp_list =
                    s_HSPListFromDistinctAlignments(&alignments[query_index],
                                                    matchingSeq.index,
                                                    queryInfo);
                if (hsp_list == NULL) {
                    status_code = -1;
                    goto match_loop_cleanup;
                }
                if (hsp_list->hspcnt > 1) {
                    s_HitlistReapContained(hsp_list->hsp_array,
                                           &hsp_list->hspcnt);
                }
                status_code =
                    s_HitlistEvaluateAndPurge(&bestScore, &bestEvalue,
                                              hsp_list,
                                              seqSrc,
                                              matchingSeq.length,
                                              program_number,
                                              queryInfo, query_index,
                                              sbp, hitParams,
                                              pvalueForThisPair, LambdaRatio,
                                              matchingSeq.index);
                if (status_code != 0) {
                    goto query_loop_cleanup;
                }
                if (bestEvalue <= hitParams->options->expect_value &&
                    BlastCompo_HeapWouldInsert(&redoneMatches[query_index],
                                               bestEvalue, bestScore,
                                               thisMatch->oid)) {
                    /* The best alignment is significant */
                    s_HSPListNormalizeScores(hsp_list,
                                             kbp->Lambda, kbp->logK,
                                             localScalingFactor);
                     s_ComputeNumIdentities(queryBlk, queryInfo, seqSrc,
                                           hsp_list, scoringParams->options,
 						genetic_code_string);
                    status_code =
                        BlastCompo_HeapInsert(&redoneMatches[query_index],
                                              hsp_list, bestEvalue,
                                              bestScore, thisMatch->oid,
                                              &discardedAligns);
                    if (status_code == 0) {
                        hsp_list = NULL;
                    } else {
                        goto query_loop_cleanup;
                    }
                    if (discardedAligns != NULL) {
                        Blast_HSPListFree(discardedAligns);
                    }
                }
query_loop_cleanup:
                Blast_HSPListFree(hsp_list);
                if (status_code != 0) {
                    goto match_loop_cleanup;
                }
            } /* end if any alignments were found */
        } /* end loop over queries */
match_loop_cleanup:
        if (status_code != 0) {
            for (query_index = 0;  query_index < numQueries;  query_index++) {
                BlastCompo_AlignmentsFree(&alignments[query_index],
                                          s_FreeEditScript);
            }
        }
        s_MatchingSequenceRelease(&matchingSeq);
        thisMatch = Blast_HSPListFree(thisMatch);
        BlastCompo_AlignmentsFree(&incoming_aligns, NULL);
        if (status_code != 0) {
            goto function_cleanup;
        }
    }
    /* end for all matching sequences */
function_cleanup:
    sfree(alignments);
    if (status_code == 0) {
        s_FillResultsFromCompoHeaps(results, redoneMatches,
                                    hitParams->options->hitlist_size);
    } else {
        if (redoneMatches != NULL) {
            s_ClearHeap(&redoneMatches[0]);
        }
    }
    free(query_info);
    Blast_RedoAlignParamsFree(&redo_align_params);
    if (redoneMatches != NULL) {
        for (query_index = 0;  query_index < numQueries;  query_index++) {
            BlastCompo_HeapRelease(&redoneMatches[query_index]);
        }
        sfree(redoneMatches); redoneMatches = NULL;
    }
    if (smithWaterman) {
        Blast_ForbiddenRangesRelease(&forbidden);
    }
    if (gapAlign != NULL) {
        gapAlign = BLAST_GapAlignStructFree(gapAlign);
    }
    s_RestoreSearch(sbp, scoringParams, savedParams, queryBlk->length,
                    positionBased, compo_adjust_mode);
    s_SavedParametersFree(&savedParams);
    Blast_CompositionWorkspaceFree(&NRrecord);

    return (Int2) status_code;
}
