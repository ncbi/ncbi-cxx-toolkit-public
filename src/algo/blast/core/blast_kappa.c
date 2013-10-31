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
    const BlastContextInfo * query_context,
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
        if (seqSrc){
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
        }
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
            if (hsp2->query.frame == hsp1->query.frame &&
                hsp2->subject.frame == hsp1->subject.frame) {
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
 * @param hsp_list   The hsp_list to populate
 * @param alignments A list of distinct alignments; freed before return [in]
 * @param oid        Ordinal id of a database sequence [in]
 * @param queryInfo  information about all queries in this search [in]
 * @param frame     query frame
 * @return Allocated and filled BlastHSPList structure.
 */
static int
s_HSPListFromDistinctAlignments(BlastHSPList *hsp_list,
                                BlastCompo_Alignment ** alignments,
                                int oid,
                                const BlastQueryInfo* queryInfo,
                                int frame)
{
    int status = 0;                    /* return code for any routine called */
    static const int unknown_value = 0;   /* dummy constant to use when a
                                             parameter value is not known */
    BlastCompo_Alignment * align;  /* an alignment in the list */

    if (hsp_list == NULL) {
        return -1;
    }
    hsp_list->oid = oid;

    for (align = *alignments;  NULL != align;  align = align->next) {
        BlastHSP * new_hsp = NULL;
        GapEditScript * editScript = align->context;
        align->context = NULL;
        
        status = Blast_HSPInit(align->queryStart, align->queryEnd,
                               align->matchStart, align->matchEnd,
                               unknown_value, unknown_value,
                               align->queryIndex, 
                               frame, (Int2) align->frame, align->score,
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
    return 0;
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
 * @param context_index      the index of the query corresponding to 
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
                          const BlastQueryInfo* queryInfo,
                          int context_index,
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
            Blast_HSPListGetEvalues(program_number, queryInfo, subject_length,
                                    hsp_list, TRUE, FALSE, sbp,
                                    0.0, /* use a non-zero gap decay
                                            only when linking HSPs */
                                    1.0); /* Use scaling factor equal to
                                             1, because both scores and
                                             Lambda are scaled, so they
                                             will cancel each other. */
    }
    if (eBlastTypeBlastp == program_number ||
        eBlastTypeBlastx == program_number) {
        if ((0 <= pvalueForThisPair) && (pvalueForThisPair <= 1)) {
            s_AdjustEvaluesForComposition(hsp_list, pvalueForThisPair, seqSrc,
                                          subject_length,
                                          &queryInfo->contexts[context_index],
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
                       BLAST_SequenceBlk* subject_blk,
                       const BlastSeqSrc* seq_src,
                       BlastHSPList* hsp_list,
                       const BlastScoringOptions* scoring_options,
                       const Uint1* gen_code_string,
                       const BlastScoreBlk* sbp)
{
    Uint1* query = NULL;
    Uint1* query_nomask = NULL;
    Uint1* subject = NULL;
    const EBlastProgramType program_number = scoring_options->program_number;
    const Boolean kIsOutOfFrame = scoring_options->is_ooframe;
    const EBlastEncoding encoding = Blast_TracebackGetEncoding(program_number);
    BlastSeqSrcGetSeqArg seq_arg;
    Int2 status = 0;
    int i;
    SBlastTargetTranslation* target_t = NULL;

    if ( !hsp_list) return;

    /* Initialize the subject */
    if (seq_src){
        memset((void*) &seq_arg, 0, sizeof(seq_arg));
        seq_arg.oid = hsp_list->oid;
        seq_arg.encoding = encoding;
        seq_arg.check_oid_exclusion = TRUE;
        status = BlastSeqSrcGetSequence(seq_src, (void*) &seq_arg);
        ASSERT(status == 0);
        (void)status; /* to pacify compiler warning */

        if (program_number == eBlastTypeTblastn) {
            subject_blk = seq_arg.seq;
    	BlastTargetTranslationNew(subject_blk, gen_code_string, eBlastTypeTblastn,
                kIsOutOfFrame, &target_t);
        }
        else {
            subject = seq_arg.seq->sequence;
        }
    } else {
        subject = subject_blk->sequence;
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
            status = Blast_HSPGetNumIdentitiesAndPositives(query, target_sequence, hsp, scoring_options, 0, sbp);
        }
        else
            status = Blast_HSPGetNumIdentitiesAndPositives(query_nomask, subject, hsp, scoring_options, 0, sbp);

        ASSERT(status == 0);
    }
    target_t = BlastTargetTranslationFree(target_t);
    if (seq_src) {
        BlastSeqSrcReleaseSequence(seq_src, (void*) &seq_arg);
        BlastSequenceBlkFree(seq_arg.seq);
    }
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
 * @param self                 the array of alignment to be filled
 * @param numAligns            number of alignments
 * @param hsp_array             an array of HSPs
 * @param hspcnt                the length of hsp_array
 * @param init_context          the initial context to process
 * @param queryInfo            information about the concatenated query
 * @param localScalingFactor    the amount by which this search is scaled
 *
 * @return the new list of alignments; or NULL if there is an out-of-memory
 *         error (or if the original array is empty)
 */
static int
s_ResultHspToDistinctAlign(BlastCompo_Alignment **self,
                           int *numAligns,
                           BlastHSP * hsp_array[], Int4 hspcnt,
                           int init_context,
                           const BlastQueryInfo* queryInfo,
                           double localScalingFactor)
{
    BlastCompo_Alignment * tail[6];        /* last element in aligns */
    int hsp_index;                             /* loop index */
    int frame_index;

    for (frame_index = 0; frame_index < 6; frame_index++) {
        tail[frame_index] = NULL;
        numAligns[frame_index] = 0;
    } 
    
    for (hsp_index = 0;  hsp_index < hspcnt;  hsp_index++) {
        BlastHSP * hsp = hsp_array[hsp_index]; /* current HSP */
        BlastCompo_Alignment * new_align;      /* newly-created alignment */
        frame_index = hsp->context - init_context;
        ASSERT(frame_index < 6 && frame_index >= 0);
        /* Incoming alignments will have coordinates of the query
           portion relative to a particular query context; they must
           be shifted for used in the composition_adjustment library.
        */
        new_align =
            BlastCompo_AlignmentNew((int) (hsp->score * localScalingFactor),
                                    eDontAdjustMatrix,
                                    hsp->query.offset, hsp->query.end, hsp->context,
                                    hsp->subject.offset, hsp->subject.end, 
                                    hsp->subject.frame, hsp);
        if (new_align == NULL) /* out of memory */
            return -1;
        if (tail[frame_index] == NULL) { /* if the list aligns is empty; */
            /* make new_align the first element in the list */
            self[frame_index] = new_align;
        } else {
            /* otherwise add new_align to the end of the list */
            tail[frame_index]->next = new_align;
        }
        tail[frame_index] = new_align;
        numAligns[frame_index]++;
    }
    return 0;
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
        if (self->index >=0) {
            BlastKappa_SequenceInfo * local_data = self->local_data;
            if (self->length > 0) {
                BlastSeqSrcReleaseSequence(local_data->seq_src,
                                   &local_data->seq_arg);
                BlastSequenceBlkFree(local_data->seq_arg.seq);
            }
            free(self->local_data);
        }
        self->local_data = NULL;
    }
}

/**
 * Do a simple gapped extension to the right from the beginning of query and
 * subject ranges examining only matches and mismatches. The extension stops
 * when there are more than max_shift mismatches or mismatches or gaps are not
 * followed by two identical matches. This is a simplified version of the
 * Danielle and Jean Thierry-Miegs' jumper
 * alignment implemented in NCBI Magic
 * http://www.ncbi.nlm.nih.gov/IEB/Research/Acembly/Download/Downloads.html
 *
 * @param query_seq Query sequence [in]
 * @param query_len Query length [in]
 * @param subject_seq Subject sequence [in]
 * @param subject_len Subject length [in]
 * @param max_shift Maximum number of mismatches or gaps, extension stops if
 *        this number is reached [in]
 * @param query_ext_len Extension length on the query [out]
 * @param subject_ext_len Extension length on the subject [out]
 * @param align_len Alignment length [out]
 * @return Number of identical residues
 */
static int s_ExtendRight(Uint1* query_seq, int query_len,
                         Uint1* subject_seq, int subject_len,
                         int max_shift,
                         int* query_ext_len, int* subject_ext_len,
                         int* align_len)
{
    int num_identical = 0;
    int q_pos, s_pos;
    int gaps_in_query = 0;
    int gaps_in_subject = 0;
    q_pos = 0;
    s_pos = 0;
    while (q_pos < query_len && s_pos < subject_len) {
        int n;
        int match = 0;

        while (q_pos < query_len && s_pos < subject_len
               && query_seq[q_pos] == subject_seq[s_pos]) {

            num_identical++;
            q_pos++;
            s_pos++;
        }

        /* try to skip mismatches or gaps */
        for (n=1; n < max_shift && q_pos + n + 1 < query_len
                 && s_pos + n + 1 < subject_len && !match; n++) {

            /* mismatches */
            if (query_seq[q_pos + n] == subject_seq[s_pos + n]
                && query_seq[q_pos + n + 1] == subject_seq[s_pos + n + 1]) {
                
                /* we have already checked that two positions behind mismatches
                   match so we can advance further */
                q_pos += n + 2;
                s_pos += n + 2;
                num_identical += 2;
                match = 1;
            }

            /* gap in subject */
            if (!match && query_seq[q_pos + n] == subject_seq[s_pos]
                && query_seq[q_pos + n + 1] == subject_seq[s_pos + 1]) {
                
                q_pos += n + 2;
                s_pos += 2;
                num_identical += 2;
                gaps_in_subject += n;
                match = 1;
            }

            /* gap in query */
            if (!match && query_seq[q_pos] == subject_seq[s_pos + n]
                && query_seq[q_pos + 1] == subject_seq[s_pos + n + 1]) {

                q_pos += 2;
                s_pos += n + 2;
                num_identical += 2;
                gaps_in_query += n;
                match = 1;
            }
        }

        if (match) {
            continue;
        }

        /* exit the loop */
        break;
    }
    *query_ext_len = q_pos;
    *subject_ext_len = s_pos;
    *align_len = q_pos > s_pos ? q_pos + gaps_in_query : s_pos + gaps_in_subject;

    return num_identical;
}


/** 
 * Extend left from the end of the sequence and subject ranges and count
 * identities. The extension stops when there are more than max_shift
 * mismatches or mismatches or gaps are not followed by two identical matches.
 * See description for s_ExtendRight for more details.
 *
 * @param query_seq Query sequence [in]
 * @param query_len Query length [in]
 * @param subject_seq Subject sequence [in]
 * @param subject_len Subject length [in]
 * @param max_shift Maximum number of mismatches or gaps, extension stops if
 *        this number is reached [in]
 * @param query_ext_len Extension length on the query [out]
 * @param subject_ext_len Extension length on the subject [out]
 * @param align_len Alignment length [out]
 * @return Number of identical residues
 */
static int s_ExtendLeft(Uint1* query_seq, int query_len,
                        Uint1* subject_seq, int subject_len,
                        int max_shift,
                        int* query_ext_len, int* subject_ext_len,
                        int* align_len)
{
    int q_pos = query_len - 1;
    int s_pos = subject_len - 1;
    int num_identical = 0;
    int gaps_in_query = 0;
    int gaps_in_subject = 0;
    while (q_pos >= 0 && s_pos >= 0) {
        int n;
        int match = 0;

        /* process identies */
        while (q_pos > 0 && s_pos > 0 && query_seq[q_pos] == subject_seq[s_pos]) {
            num_identical++;
            q_pos--;
            s_pos--;
        }

        /* try to skip mismatches or gaps */
        for (n=1;n < max_shift && q_pos - n - 1 > 0 && s_pos - n - 1 > 0
                 && !match; n++) {

            /* mismatch */
            if (query_seq[q_pos - n] == subject_seq[s_pos - n]
                && query_seq[q_pos - n - 1] == subject_seq[s_pos - n - 1]) {
                q_pos -= n + 2;
                s_pos -= n + 2;
                num_identical += 2;
                match = 1;
            }

            /* gap in subject */
            if (!match && query_seq[q_pos - n] == subject_seq[s_pos]
                && query_seq[q_pos - n - 1] == subject_seq[s_pos - 1]) {
                q_pos -= n + 2;
                s_pos -= 2;
                num_identical += 2;
                gaps_in_subject += n;
                match = 1;
            }

            /* gap in query */
            if (!match && query_seq[q_pos] == subject_seq[s_pos - n]
                && query_seq[q_pos - 1] == subject_seq[s_pos - n - 1]) {
                q_pos -= 2;
                s_pos -= n + 2;
                num_identical += 2;
                gaps_in_query += n;
                match = 1;
            }
        }

        if (match) {
            continue;
        }

        break;
    }
    *query_ext_len = query_len - q_pos - 1;
    *subject_ext_len = subject_len - s_pos - 1;
    *align_len += *query_ext_len > *subject_ext_len ?
        *query_ext_len + gaps_in_query : *subject_ext_len + gaps_in_subject;

    return num_identical;
}


/** 
 * Get hash for a word of word_size residues assuming 28-letter alphabet
 *
 * @param data Sequence [in]
 * @param word_size Word size [in]
 * @return Hash value
 */
static Uint8 s_GetHash(const Uint1* data, int word_size)
{
    Uint8 hash = 0;
    int k;
    for (k=0;k < word_size;k++) {
        hash <<= 5;
        hash += (Int8)data[k];
    }
    return hash;
}


/** 
 * Find a local number of identical residues in two aligned sequences by
 * finding word matches and doing a simple gapped extensions from the word hits
 *
 * @param query_seq Query sequence [in]
 * @param query_hashes Array of query words with index of each word
 *        corresponding to word position in the query [in]
 * @param query_len Query length [in]
 * @param subject_seq Subject sequence [in]
 * @param subject_len Subject length [in]
 * @param max_shift Maximum number of local mismatches or gaps for extensions
 *        [in]
 * @return Number of identical residues
 */
static int s_FindNumIdentical(Uint1* query_seq,
                              const Uint8* query_hashes,
                              int query_len,
                              Uint1* subject_seq,
                              int subject_len,
                              int max_shift)
{
    int word_size = 8;         /* word size for k-mer matching */
    Uint8 hash;
    Uint8 mask = 0xFFFFFFFFFF; /* mask for computing hash values */
    int query_from = 0;
    int subject_from = 0;

    int s_pos;                 /* position in the subject sequence */
    int num_identical = 0;     /* number of identical residues found */
    Boolean match = FALSE;

    /* if query or subject length is smaller than word size, exit */
    if (!query_seq || !query_hashes || !subject_seq
        || query_len < word_size || subject_len < word_size) {

        return 0;
    }

    /* for each subject position */
    for (s_pos = 0; s_pos < subject_len - word_size; s_pos++) {
        int q_pos;

        /* find word hash */
        if (s_pos == 0 || match) {
            hash = s_GetHash(&subject_seq[s_pos], word_size);
        }
        else {
            hash <<= 5;
            hash &= mask;
            hash += subject_seq[s_pos + word_size - 1];
        }

        /* find matching query word; index of hash is position of the word 
           the query */
        for (q_pos = query_from;q_pos < query_len - word_size; q_pos++) {
            if (query_hashes[q_pos] == hash) {
                break;
            }
        }

        /* if match */
        if (q_pos < query_len - word_size) {
            int query_start = q_pos;
            int subject_start = s_pos;

            int query_left_len, query_right_len;
            int subject_left_len, subject_right_len;
            int align_len_left, align_len_right;
            
            match = TRUE;
            num_identical += word_size;

            /* extend left from word match */
            num_identical += s_ExtendLeft(query_seq + query_start - 1,
                                          query_start - query_from,
                                          subject_seq + subject_start - 1,
                                          subject_start - subject_from,
                                          max_shift,
                                          &query_left_len, &subject_left_len,
                                          &align_len_left);

            /* extend right from word match */
            num_identical += s_ExtendRight(query_seq + query_start + word_size,
                                       query_len - query_start - word_size,
                                       subject_seq + subject_start + word_size,
                                       subject_len - subject_start - word_size,
                                       max_shift,
                                       &query_right_len, &subject_right_len,
                                       &align_len_right);


            /* disregard already matched and extended words when matching
               further positions */

            query_from = query_start + word_size + query_right_len;
            subject_from = subject_start + word_size + subject_right_len;
            /* s_pos will be incremented in the loop */
            s_pos = subject_from - 1;
        }
        else {
            match = FALSE;
        }
    }

    return num_identical;
}

/**
 * Test whether the aligned parts of two sequences that
 * have a high-scoring gapless alignment are nearly identical.
 *
 * First extend from the left end of the query and subject ranges and stop if
 * there are too manu mismatches. Then extend from the right end. Then for the
 * remaining protion of ths sequences find matching words and extend left and
 * right from the word hit. Repeat the last steo until the whole alignment
 * ranges are processed.
 *
 * @params seqData Subject sequence [in]
 * @params seqOffse Starting offset of the subject sequence in alignment data
 *        [in]
 * @params queryData Query sequence [in]
 * @params queryOffset Starting offset of the query sequence in alignment data
 *         [in]
 * @param query_words Array of query words with word index corresponding to
 *        word's position in the query [in]
 * @param align Alignment data [in]
 * @return True if sequence parts are nearly identical, false otherwise
 */
static Boolean
s_TestNearIdentical(const BlastCompo_SequenceData* seqData,
                    const int seqOffset,
                    const BlastCompo_SequenceData* queryData,
                    const int queryOffset,
                    const Uint8* query_words,
                    const BlastCompo_Alignment* align)
{
    int qStart = align->queryStart - queryOffset;
    /* align->queryEnd points to one position past alignment end */
    int qEnd = align->queryEnd - queryOffset - 1;
    int sStart = align->matchStart - seqOffset;
    int sEnd = align->matchEnd - seqOffset - 1;
    const double kMinFractionNearIdentical = 0.96;
    int max_shift = 8;

    int query_len = qEnd - qStart + 1;
    int subject_len = sEnd - sStart + 1;
    int align_len = MIN(query_len, subject_len);

    int query_left_len = 0;
    int subject_left_len = 0;
    int query_right_len = 0;
    int subject_right_len = 0;
    int align_left_len = 0;
    int align_right_len = 0;

    double fraction_identical;

    /* first find number of identies going from the beginning of the query
       and subject ranges */
    int num_identical = s_ExtendRight(queryData->data + qStart, query_len,
                                      seqData->data + sStart, subject_len,
                                      max_shift,
                                      &query_right_len, &subject_right_len,
                                      &align_right_len);

    /* if the whole query range was processed return near identical status */
    if (query_right_len >= query_len || subject_right_len >= subject_len) {
        fraction_identical = (double)num_identical / (double)align_len;
        ASSERT(fraction_identical - 1.0 < 1e-10);
        return fraction_identical > kMinFractionNearIdentical;
    }

    /* find the number of identies going from the end of the query and subject
       ranges */
    num_identical += s_ExtendLeft(queryData->data + qStart + query_right_len,
                                  query_len - query_right_len,
                                  seqData->data + sStart + subject_right_len,
                                  subject_len - subject_right_len,
                                  max_shift,
                                  &query_left_len, &subject_left_len,
                                  &align_left_len);

    /* if the whole alignment ranges where covered, return the near identical
       status */
    if (query_left_len + query_right_len >= query_len
        || subject_left_len + subject_right_len >= subject_len) {

        fraction_identical = (double)num_identical / (double)(align_len);
        ASSERT(fraction_identical - 1.0 < 1e-10);
        return fraction_identical > kMinFractionNearIdentical;
    }

    /* find the number of identical matches in the middle portion of the
       alignment ranges */
    num_identical += s_FindNumIdentical(queryData->data + qStart + query_right_len,
                            query_words + qStart + query_right_len,
                            query_len - query_left_len - query_right_len,
                            seqData->data + sStart + subject_right_len,
                            subject_len - subject_left_len - subject_right_len,
                            max_shift);

    fraction_identical = (double)num_identical / (double)align_len;
    ASSERT(fraction_identical - 1.0 < 1e-10);
    if (fraction_identical > kMinFractionNearIdentical) {
        return TRUE;
    }
    else {
        return FALSE;
    }
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
                    EBlastProgramType program_name,
                    Boolean* is_seq_biased)
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
    if (is_seq_biased) {
        *is_seq_biased = (mask_seqloc != NULL);
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
                             const BlastCompo_SequenceRange * q_range,
                             BlastCompo_SequenceData * queryData,
                             const Uint8* query_words,
                             const BlastCompo_Alignment *align,
                             const Boolean shouldTestIdentical,
                             const ECompoAdjustModes compo_adjust_mode,
                             const Boolean isSmithWaterman,
                             Boolean* subject_maybe_biased)
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
            if (compo_adjust_mode
                && (!subject_maybe_biased || *subject_maybe_biased)) {

                if ( (!shouldTestIdentical)
                     || (shouldTestIdentical
                         && (!s_TestNearIdentical(seqData, range->begin,
                                                  queryData, q_range->begin,
                                                  query_words, align)))) {

                    status = s_DoSegSequenceData(seqData, eBlastTypeTblastn,
                                                 subject_maybe_biased);
                    if (status != 0) {
                        free(seqData->buffer);
                        seqData->buffer = NULL;
                        seqData->data = NULL;
                        seqData->length = 0;
                    }
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
                          const BlastCompo_SequenceRange * q_range,
                          BlastCompo_SequenceData * queryData,
                          const Uint8* query_words,
                          const BlastCompo_Alignment *align,
                          const Boolean shouldTestIdentical,
                          const ECompoAdjustModes compo_adjust_mode,
                          const Boolean isSmithWaterman,
                          Boolean* subject_maybe_biased)

{
    int status = 0;       /* return status */
    Int4       idx;       /* loop index */
    Uint1     *origData;  /* the unfiltered data for the sequence */
    /* BLAST-specific sequence information */
    BlastKappa_SequenceInfo * local_data = self->local_data;
    BLAST_SequenceBlk * seq = self->local_data;

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

    origData = (self->index >= 0) ? local_data->seq_arg.seq->sequence
                            : seq->sequence;
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
        if (compo_adjust_mode
            && (!subject_maybe_biased || *subject_maybe_biased)) {

            if ( (!shouldTestIdentical)
                 || (shouldTestIdentical
                     && (!s_TestNearIdentical(seqData, 0, queryData,
                                              q_range->begin, query_words,
                                              align)))) {

                status = s_DoSegSequenceData(seqData, eBlastTypeBlastp,
                                             subject_maybe_biased);
            }
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
                   const BlastCompo_SequenceRange * s_range,
                   BlastCompo_SequenceData * seqData,
                   const BlastCompo_SequenceData * query,
                   const BlastCompo_SequenceRange * q_range,
                   BlastCompo_SequenceData * queryData,
                   const Uint8* query_words,
                   const BlastCompo_Alignment *align,
                   const Boolean shouldTestIdentical,
                   const ECompoAdjustModes compo_adjust_mode,
                   const Boolean isSmithWaterman,
                   Boolean* subject_maybe_biased)
{
    Int4 idx;
    BlastKappa_SequenceInfo * seq_info = self->local_data;
    Uint1 *origData = query->data + q_range->begin;
    /* Copy the query sequence (necessary for SEG filtering.) */
    queryData->length = q_range->end - q_range->begin;
    queryData->buffer = calloc((queryData->length + 2), sizeof(Uint1));
    queryData->data   = queryData->buffer + 1;

    for (idx = 0;  idx < queryData->length;  idx++) {
        /* Copy the sequence data, replacing occurrences of amino acid
         * number 24 (Selenocysteine) with number 21 (Undetermined or
         * atypical). */
        queryData->data[idx] = (origData[idx] != 24) ? origData[idx] : 21;
    }
    if (seq_info && seq_info->prog_number ==  eBlastTypeTblastn) {
        /* The sequence must be translated. */
        return s_SequenceGetTranslatedRange(self, s_range, seqData,
                                            q_range, queryData, query_words,
                                            align, shouldTestIdentical,
                                            compo_adjust_mode, isSmithWaterman,
                                            subject_maybe_biased);
    } else {
        return s_SequenceGetProteinRange(self, s_range, seqData, 
                                         q_range, queryData, query_words,
                                         align, shouldTestIdentical,
                                         compo_adjust_mode, isSmithWaterman,
                                         subject_maybe_biased);
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
    /* Auxiliary structure for computing gapped alignments */
    BlastGapAlignStruct* gapAlign = context->gap_align;
    /* The preliminary gapped HSP that were are recomputing */
    BlastHSP * hsp = in_align->context;

    /* suppress unused parameter warnings; this is a callback
       function, so these parameter cannot be deleted */
    (void) ccat_query_length;
    (void) full_subject_length;

    /* Use the starting point supplied by the HSP. */
    q_start = hsp->query.gapped_start - query_range->begin;
    s_start = hsp->subject.gapped_start - subject_range->begin;

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


/* Create an array of 8-mers for a sequence, such that index of each 8-mer
   is the same as its position in the query */
static int
s_CreateWordArray(const Uint1* seq_data, Int4 seq_len, Uint8** words)
{
    int word_size = 8;         /* word size for k-mer matching */
    Uint8* query_hashes;       /* list of hashes for query words */
    Uint8 mask = 0xFFFFFFFFFF; /* mask for computing hash values */

    int i;

    /* if query or subject length is smaller than word size, exit */
    if (!seq_data || !words || seq_len < word_size) {
        return -1;
    }

    query_hashes = (Uint8*)calloc((seq_len - word_size + 1),
                                  sizeof(Uint8));
    *words = query_hashes;

    if (!query_hashes) {
        return -1;
    }

    
    /* find query word hashes */
    query_hashes[0] = s_GetHash(&seq_data[0], word_size);
    for (i = 1; i < seq_len - word_size; i++) {
        query_hashes[i] = query_hashes[i - 1];
        query_hashes[i] <<= 5;
        query_hashes[i] &= mask;
        query_hashes[i] += (Uint8)seq_data[i + word_size - 1];
    }

    return 0;
}


static void s_FreeBlastCompo_QueryInfoArray(BlastCompo_QueryInfo** query_info,
                                            int num_queries)
{
    int i;

    if (!query_info) {
        return;
    }

    for (i = 0;i < num_queries;i++) {
        if ((*query_info)[i].words) {
            free((*query_info)[i].words);
        }
    }
    
    free(*query_info);
    *query_info = NULL;
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
s_GetQueryInfo(Uint1 * query_data, const BlastQueryInfo * blast_query_info, Boolean skip)
{
    int i;                   /* loop index */
    BlastCompo_QueryInfo *
        compo_query_info;    /* the new array */
    int num_queries;         /* the number of queries/elements in
                                compo_query_info */

    num_queries = blast_query_info->last_context + 1;
    compo_query_info = calloc(num_queries, sizeof(BlastCompo_QueryInfo));
    if (compo_query_info != NULL) {
        for (i = 0;  i < num_queries;  i++) {
            BlastCompo_QueryInfo * query_info = &compo_query_info[i];
            const BlastContextInfo * query_context = &blast_query_info->contexts[i];

            query_info->eff_search_space =
                (double) query_context->eff_searchsp;
            query_info->origin = query_context->query_offset;
            query_info->seq.data = &query_data[query_info->origin];
            query_info->seq.length = query_context->query_length;
            query_info->words = NULL;

            s_CreateWordArray(query_info->seq.data, query_info->seq.length,
                              &query_info->words);
            if (! skip) {
                Blast_ReadAaComposition(&query_info->composition, BLASTAA_SIZE,
                                        query_info->seq.data,
                                        query_info->seq.length);
            }
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


/* Bit score per alignment position threshold for preliminaru near identical
   test */
#define NEAR_IDENTICAL_BITS_PER_POSITION (1.8)

/** 
 * Read the parameters required for the Blast_RedoOneMatch* functions from
 * the corresponding parameters in standard BLAST datatypes.  Return a new
 * object representing these parameters.
 */
static Blast_RedoAlignParams *
s_GetAlignParams(BlastKappa_GappingParamsContext * context,
                 BLAST_SequenceBlk * queryBlk,
                 const BlastQueryInfo* queryInfo,
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
    int query_is_translated   = context->prog_number == eBlastTypeBlastx;
    /* is this a positiion-based search */
    Boolean positionBased = (Boolean) (context->sbp->psi_matrix != NULL);
    /* will BLAST_LinkHsps be called to assign e-values */
    Boolean do_link_hsps = (hitParams->do_sum_stats);
    ECompoAdjustModes compo_adjust_mode =
        (ECompoAdjustModes) extendParams->options->compositionBasedStats;

    /* per position bit score cutoff for testing whether sequences are
       near identical */
    double near_identical_cutoff_bits = NEAR_IDENTICAL_BITS_PER_POSITION;

    /* score block is already scaled by context->localScalingFactor */
    double near_identical_cutoff;
    ASSERT(context->sbp->kbp_gap[queryInfo->first_context]);
    near_identical_cutoff =
        (near_identical_cutoff_bits * NCBIMATH_LN2)
        / context->sbp->kbp_gap[queryInfo->first_context]->Lambda;
    
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
                                        queryInfo->last_context + 1);
    if (gapping_params == NULL) {
        return NULL;
    } else {
        return
            Blast_RedoAlignParamsNew(&scaledMatrixInfo, &gapping_params,
                                     compo_adjust_mode, positionBased,
                                     query_is_translated,
                                     subject_is_translated,
                                     queryInfo->max_length, cutoff_s, cutoff_e,
                                     do_link_hsps, &redo_align_callbacks,
                                     near_identical_cutoff);
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

extern void
BLAST_SetupPartialFetching(EBlastProgramType program_number,
                           BlastSeqSrc* seq_src, 
                           const BlastHSPList** hsp_list,
                           Int4 num_hsplists);

/**
 *  Recompute alignments for each match found by the gapped BLAST
 *  algorithm.
 */
Int2
Blast_RedoAlignmentCore(EBlastProgramType program_number,
                        BLAST_SequenceBlk * queryBlk,
                        const BlastQueryInfo* queryInfo,
                        BlastScoreBlk* sbp,
                        BLAST_SequenceBlk * subjectBlk,
                        const BlastSeqSrc* seqSrc,
                        Int4 default_db_genetic_code,
                        BlastHSPList * thisMatch,
                        BlastHSPStream* hsp_stream,
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
    /* context number */
    int context_index;
    /* frame number */
    int frame_index;
    /* number of queries in the concatenated query */
    int numQueries = queryInfo->num_queries;
    /* number of contexts in the concatenated query */
    int numContexts = queryInfo->last_context + 1;
    /* number of contexts within a query */
    int numFrames = (program_number == eBlastTypeBlastx) ? 6:1;
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
    /* existing alignments for a match */
    BlastCompo_Alignment ** incoming_align_set = NULL;
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

    ASSERT(program_number == eBlastTypeBlastp   ||
           program_number == eBlastTypeTblastn  ||
           program_number == eBlastTypeBlastx   ||
           program_number == eBlastTypePsiBlast ||
           program_number == eBlastTypeRpsBlast);

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
        s_SavedParametersNew(queryInfo->max_length, numContexts,
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
    s_RescaleSearch(sbp, scoringParams, numContexts, localScalingFactor);
    status_code =
        BLAST_GapAlignStructNew(scoringParams, extendParams,
                                (seqSrc) ?  BlastSeqSrcGetMaxSeqLen(seqSrc) 
                                         :  subjectBlk->length,
                                sbp, &gapAlign);
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

    query_info = s_GetQueryInfo(queryBlk->sequence, queryInfo, 
                                (program_number == eBlastTypeBlastx));
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
    alignments = calloc(numContexts, sizeof(BlastCompo_Alignment *));
    incoming_align_set = calloc(numFrames, sizeof(BlastCompo_Alignment *));
    if (alignments == NULL || incoming_align_set == NULL) {
        status_code = -1;
        goto function_cleanup;
    }
   
    while (1) {
        int numAligns[6];
        Blast_KarlinBlk * kbp = NULL;
        BlastCompo_MatchingSequence matchingSeq = {0,};
        BlastHSPList * hsp_list = Blast_HSPListNew(0);
        double best_evalue;   
        Int4 best_score;
        void * discarded_aligns = NULL;
     
        if (seqSrc
           && BlastHSPStreamRead(hsp_stream, &thisMatch) == kBlastHSPStream_Eof) 
           break;

        if (thisMatch->hsp_array == NULL) {
            if (seqSrc) continue;
            break;
        }

        if (BlastCompo_EarlyTermination(thisMatch->best_evalue,
                                        redoneMatches, numQueries)) {
            Blast_HSPListFree(thisMatch);
            if (seqSrc) continue;
            break;
        }

        query_index = thisMatch->query_index;
        context_index = query_index * numFrames;
        /* Get the sequence for this match */
        if (seqSrc && BlastSeqSrcGetSupportsPartialFetching(seqSrc)) {
            BLAST_SetupPartialFetching(program_number, (BlastSeqSrc*)seqSrc, 
                                       (const BlastHSPList**)&thisMatch, 1);
        }

        if (subjectBlk) {
            matchingSeq.length = subjectBlk->length;
            matchingSeq.index = -1;
            matchingSeq.local_data = subjectBlk;
        } else {
            status_code =
                s_MatchingSequenceInitialize(&matchingSeq, program_number,
                                         seqSrc, default_db_genetic_code,
                                         thisMatch->oid);
            if (status_code != 0) {
                /* some sequences may have been excluded by membit filtering 
                   so this is not really an exception */
                status_code = 0;
                goto match_loop_cleanup;
            }
        }
        status_code =
            s_ResultHspToDistinctAlign(incoming_align_set, numAligns,
                                       thisMatch->hsp_array, 
                                       thisMatch->hspcnt, context_index,
                                       queryInfo, localScalingFactor);
        if (status_code != 0) {
            goto match_loop_cleanup;
        }

        for (frame_index=0; frame_index<numFrames; frame_index++, context_index++) {
            incoming_aligns = incoming_align_set[frame_index];
            if (!incoming_aligns) continue;
            /* All alignments in thisMatch should be to the same query */
            kbp = sbp->kbp_gap[context_index];
            if (smithWaterman) {
                status_code =
                    Blast_RedoOneMatchSmithWaterman(alignments,
                                                redo_align_params,
                                                incoming_aligns,
                                                numAligns[frame_index],
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
                                   incoming_aligns, numAligns[frame_index],
                                   kbp->Lambda, &matchingSeq,
                                   -1, query_info,
                                   numContexts, matrix, BLASTAA_SIZE,
                                   NRrecord, &pvalueForThisPair,
                                   compositionTestIndex,
                                   &LambdaRatio);
            }

            if (status_code != 0) {
                goto match_loop_cleanup;
            }

            if (alignments[context_index] != NULL) {
                Int2 qframe = frame_index;
                if (program_number == eBlastTypeBlastx) {
                    if (qframe < 3) qframe++;
                    else qframe = 2-qframe;
                }
                status_code =             
                    s_HSPListFromDistinctAlignments(hsp_list,
                                          &alignments[context_index],
                                          matchingSeq.index,
                                          queryInfo, qframe);
                if (status_code) {
                    goto match_loop_cleanup;
                }
            }
            BlastCompo_AlignmentsFree(&incoming_aligns, NULL);
            incoming_align_set[frame_index] = NULL;
        }

        if (hsp_list->hspcnt > 1) {
            s_HitlistReapContained(hsp_list->hsp_array,
                                   &hsp_list->hspcnt);
        }
        status_code =
            s_HitlistEvaluateAndPurge(&best_score, &best_evalue,
                                              hsp_list,
                                              seqSrc,
                                              matchingSeq.length,
                                              program_number,
                                              queryInfo, context_index,
                                              sbp, hitParams,
                                              pvalueForThisPair, LambdaRatio,
                                              matchingSeq.index);
        if (status_code != 0) {
            goto query_loop_cleanup;
        }
        if (best_evalue <= hitParams->options->expect_value) {
            /* The best alignment is significant */
            s_HSPListNormalizeScores(hsp_list, kbp->Lambda, kbp->logK,
                                             localScalingFactor);
            s_ComputeNumIdentities(queryBlk, queryInfo, subjectBlk, seqSrc,
                                           hsp_list, scoringParams->options,
                                           genetic_code_string, sbp);
            if (!seqSrc) goto query_loop_cleanup;
            if ( BlastCompo_HeapWouldInsert(&redoneMatches[query_index],
                                               best_evalue, best_score,
                                               thisMatch->oid)) {
                status_code = BlastCompo_HeapInsert(&redoneMatches[query_index],
                                              hsp_list, best_evalue,
                                              best_score, thisMatch->oid,
                                              &discarded_aligns);
                if (status_code == 0) hsp_list = NULL;
            } else {
                hsp_list = Blast_HSPListFree(hsp_list);
            }

            if (status_code) goto query_loop_cleanup;
            if (discarded_aligns != NULL) {
                Blast_HSPListFree(discarded_aligns);
            }
        }
query_loop_cleanup:
match_loop_cleanup:
        if (seqSrc) {
            thisMatch = Blast_HSPListFree(thisMatch);
        } else {
            Blast_HSPListSwap(thisMatch, hsp_list);
            thisMatch->oid = hsp_list->oid;
        }
        hsp_list = Blast_HSPListFree(hsp_list);

        if (status_code != 0) {
            for (query_index = 0;  query_index < numContexts;  query_index++) {
                BlastCompo_AlignmentsFree(&alignments[query_index],
                                          s_FreeEditScript);
            }
        }
        s_MatchingSequenceRelease(&matchingSeq);
        BlastCompo_AlignmentsFree(&incoming_aligns, NULL);
        if (status_code != 0 || !seqSrc) {
            goto function_cleanup;
        }
    }
    /* end for all matching sequences */
function_cleanup:
    sfree(alignments);
    sfree(incoming_align_set);
    if (seqSrc && status_code == 0) {
        s_FillResultsFromCompoHeaps(results, redoneMatches,
                                    hitParams->options->hitlist_size);
    } else {
        if (redoneMatches != NULL) {
            s_ClearHeap(&redoneMatches[0]);
        }
    }
    s_FreeBlastCompo_QueryInfoArray(&query_info, numQueries);
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
