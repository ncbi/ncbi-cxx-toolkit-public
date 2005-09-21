static char const rcsid[] = "$Id$";

/* ===========================================================================
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
* ===========================================================================*/

/**
 * @file blast_adjust_scores.c
 *
 * Authors: E. Michael Gertz, Alejandro Schaffer, Yi-Kuo Yu
 *
 * Sets up the optimization problem for conditional score matrix
 * adjustment based on relative entropy of the two matching sequences.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algo/blast/core/blast_toolkit.h>
#include <algo/blast/composition_adjustment/composition_adjustment.h>
#include <algo/blast/composition_adjustment/optimize_target_freq.h>
#include <algo/blast/composition_adjustment/nlm_linear_algebra.h>
#include <algo/blast/composition_adjustment/blast_adjust_scores.h>

#define SCORE_BOUND            0.0000000001
#define LAMBDA_STEP_FRACTION   0.5          /* default step fraction in
                                               Newton's method */
#define INITIAL_LAMBDA         1.0
#define LAMBDA_ITERATION_LIMIT 300
#define LAMBDA_ERROR_TOLERANCE 0.0000001    /* bound on error for estimating
                                               lambda */

/* bound on error for Newton's method */
static const double kCompoAdjustErrTolerance = 0.00000001;
/* iteration limit for Newton's method */
static const int kCompoAdjustIterationLimit = 2000;


/*free memory assoicated with a record of type Blast_CompositionWorkspace*/
void
Blast_CompositionWorkspaceFree(Blast_CompositionWorkspace ** pNRrecord)
{
    Blast_CompositionWorkspace * NRrecord = *pNRrecord;
    
    if (NRrecord != NULL) {
        free(NRrecord->first_standard_freq);
        free(NRrecord->second_standard_freq);
        free(NRrecord->first_seq_freq);
        free(NRrecord->second_seq_freq);
        free(NRrecord->first_seq_freq_wpseudo);
        free(NRrecord->second_seq_freq_wpseudo);

        Nlm_DenseMatrixFree(&NRrecord->score_old);
        Nlm_DenseMatrixFree(&NRrecord->score_final);
        Nlm_DenseMatrixFree(&NRrecord->mat_final);
        Nlm_DenseMatrixFree(&NRrecord->mat_b);

        free(NRrecord);
    }
    pNRrecord = NULL;
}


/*allocate one record of type Blast_CompositionWorkspace, allocate
 * memory for its arrays, and return a pointer to the record*/
Blast_CompositionWorkspace * Blast_CompositionWorkspaceNew()
{
    Blast_CompositionWorkspace * NRrecord;        /* record to allocate
                                                    and return */
    int i;                     /* loop index */

    NRrecord = (Blast_CompositionWorkspace *) 
        malloc(sizeof(Blast_CompositionWorkspace));
    if (NRrecord == NULL) goto error_return;

    NRrecord->first_standard_freq      = NULL;
    NRrecord->second_standard_freq     = NULL;
    NRrecord->first_seq_freq           = NULL;
    NRrecord->second_seq_freq          = NULL;
    NRrecord->first_seq_freq_wpseudo   = NULL;
    NRrecord->second_seq_freq_wpseudo  = NULL;
    NRrecord->score_old                = NULL;
    NRrecord->score_final              = NULL;
    NRrecord->mat_final                = NULL;
    NRrecord->mat_b                    = NULL;
    
    NRrecord->first_standard_freq =
        (double *) malloc(COMPOSITION_ALPHABET_SIZE * sizeof(double));
    if (NRrecord->first_standard_freq == NULL) goto error_return;

    NRrecord->second_standard_freq =
        (double *) malloc(COMPOSITION_ALPHABET_SIZE * sizeof(double));
    if (NRrecord->second_standard_freq == NULL) goto error_return;

    NRrecord->first_seq_freq =
        (double *) malloc(COMPOSITION_ALPHABET_SIZE * sizeof(double));
    if (NRrecord->first_seq_freq == NULL) goto error_return;

    NRrecord->second_seq_freq =
        (double *) malloc(COMPOSITION_ALPHABET_SIZE * sizeof(double));
    if (NRrecord->second_seq_freq == NULL) goto error_return;

    NRrecord->first_seq_freq_wpseudo =
        (double *) malloc(COMPOSITION_ALPHABET_SIZE * sizeof(double));
    if (NRrecord->first_seq_freq_wpseudo == NULL) goto error_return;

    NRrecord->second_seq_freq_wpseudo =
        (double *) malloc(COMPOSITION_ALPHABET_SIZE * sizeof(double));
    if (NRrecord->second_seq_freq_wpseudo == NULL) goto error_return;

    NRrecord->score_old   = Nlm_DenseMatrixNew(COMPOSITION_ALPHABET_SIZE,
                                               COMPOSITION_ALPHABET_SIZE);
    if (NRrecord->score_old == NULL) goto error_return;
    
    NRrecord->score_final = Nlm_DenseMatrixNew(COMPOSITION_ALPHABET_SIZE,
                                               COMPOSITION_ALPHABET_SIZE);
    if (NRrecord->score_final == NULL) goto error_return;
    
    NRrecord->mat_final   = Nlm_DenseMatrixNew(COMPOSITION_ALPHABET_SIZE,
                                               COMPOSITION_ALPHABET_SIZE);
    if (NRrecord->mat_final == NULL) goto error_return;
    
    NRrecord->mat_b       = Nlm_DenseMatrixNew(COMPOSITION_ALPHABET_SIZE,
                                               COMPOSITION_ALPHABET_SIZE);
    if (NRrecord->mat_b == NULL) goto error_return;

    for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
        NRrecord->first_standard_freq[i] = 
            NRrecord->second_standard_freq[i] = 0.0;
        NRrecord->first_seq_freq[i] = NRrecord->second_seq_freq[i] = 0.0;
        NRrecord->first_seq_freq_wpseudo[i] =
            NRrecord->second_seq_freq_wpseudo[i] = 0.0;
    }

    goto normal_return;
error_return:
    Blast_CompositionWorkspaceFree(&NRrecord);
normal_return:
    return NRrecord;
}


/* initialize the joint probabilities used later from inside
   NRrecord */
int
Blast_CompositionWorkspaceInit(Blast_CompositionWorkspace * NRrecord,
                               const char *matrixName)
{
    double re_o_implicit = 0.0;    /* implicit relative entropy of
                                      starting matrix */
    int i, j;                      /* loop indices */

    if (0 == Blast_GetJointProbsForMatrix(NRrecord->mat_b,
                                          NRrecord->first_standard_freq,
                                          NRrecord->second_standard_freq,
                                          matrixName)) {
        for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
            for (j = 0;  j < COMPOSITION_ALPHABET_SIZE;  j++) {
                re_o_implicit +=
                    NRrecord->mat_b[i][j] * log(NRrecord->mat_b[i][j] /
                                                NRrecord->
                                                first_standard_freq[i] /
                                                NRrecord->
                                                second_standard_freq[j]);
                NRrecord->score_old[i][j] =
                    log(NRrecord->mat_b[i][j] /
                        NRrecord->first_standard_freq[i] /
                        NRrecord->second_standard_freq[j]);
            }
        }
        NRrecord->RE_o_implicit = re_o_implicit;
        return 0;
    } else {
        fprintf(stderr,
                "Matrix %s not currently supported for RE based adjustment\n",
                matrixName);
        return -1;
    }
}


/*compute Lambda and if flag set according return re_o_newcontext,
  otherwise return 0.0, also test for the possibility of average
  score >= 0*/
static double
Blast_CalcLambdaForComposition(Blast_CompositionWorkspace * NRrecord,
                               int compute_re,
                               double * lambdaToReturn)
{
    int iteration_count;   /* counter for number of iterations of
                              Newton's method */
    int i, j;              /* loop indices */
    double sum;            /* used to compute the sum for estimating
                              lambda */
    double lambda_error;   /* error when estimating lambda */
    double lambda;         /* scale parameter of the Extreme Value
                              Distribution of scores */
    double ave_score;      /* average score in new context */
    double slope;          /* used to compute the derivative when
                              estimating lambda */
    double re_to_return;   /* relative entropy if using old joint
                              probabilities*/

    lambda_error    = 1.0;
    *lambdaToReturn = 1.0;
    re_to_return    = 0.0;

    if (eRelEntropyOldMatrixNewContext == NRrecord->flag) {
        ave_score = 0.0;
        for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
            for (j = 0;  j < COMPOSITION_ALPHABET_SIZE;  j++) {
                ave_score +=
                    NRrecord->score_old[i][j] * NRrecord->first_seq_freq[i] *
                    NRrecord->second_seq_freq[j];
            }
        }
    }
    if ((eRelEntropyOldMatrixNewContext == NRrecord->flag) &&
        (ave_score >= (-SCORE_BOUND))) {
        /* fall back to no constraint mode when ave score becomes global
           alignment-like */
        NRrecord->flag = eUnconstrainedRelEntropy;

        printf("scoring matrix has nonnegative average score %12.8f,"
               " reset to mode 0 \n", ave_score);
    }
    /* Need to find the relative entropy here. */
    if (compute_re) {
        slope = 0.0;
        lambda = INITIAL_LAMBDA;
        while(slope <= LAMBDA_ERROR_TOLERANCE) {
            /* making sure iteration starting point belongs to nontrivial
               fixed point */
            lambda = 2.0 * lambda;
            for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
                for (j = 0;  j < COMPOSITION_ALPHABET_SIZE;  j++) {
                    if (eRelEntropyOldMatrixNewContext == NRrecord->flag) {
                        slope +=
                            NRrecord->score_old[i][j] *
                            exp(NRrecord->score_old[i][j] * lambda) *
                            NRrecord->first_seq_freq[i] *
                            NRrecord->second_seq_freq[j];
                    } else {
                        slope +=
                            NRrecord->score_final[i][j] *
                            exp(NRrecord->score_final[i][j] * lambda) *
                            NRrecord->first_seq_freq[i] *
                            NRrecord->second_seq_freq[j];
                    }
                }
            }
        }
        iteration_count = 0;
        while ((fabs(lambda_error) > LAMBDA_ERROR_TOLERANCE) &&
               (iteration_count < LAMBDA_ITERATION_LIMIT)) {
            sum = 0.0;
            slope = 0.0;
            for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
                for (j = 0;  j < COMPOSITION_ALPHABET_SIZE;  j++) {
                    if (eRelEntropyOldMatrixNewContext == NRrecord->flag) {
                        sum +=
                            exp(NRrecord->score_old[i][j] * lambda) *
                            NRrecord->first_seq_freq[i] *
                            NRrecord->second_seq_freq[j];
                        slope +=
                            NRrecord->score_old[i][j] *
                            exp(NRrecord->score_old[i][j] * lambda) *
                            NRrecord->first_seq_freq[i] *
                            NRrecord->second_seq_freq[j];
                    } else {
                        if(eUnconstrainedRelEntropy == NRrecord->flag) {
                            sum +=
                                exp(NRrecord->score_final[i][j] * lambda) *
                                NRrecord->first_seq_freq[i] *
                                NRrecord->second_seq_freq[j];
                            slope +=
                                NRrecord->score_final[i][j] *
                                exp(NRrecord->score_final[i][j] * lambda) *
                                NRrecord->first_seq_freq[i] *
                                NRrecord->second_seq_freq[j];
                        }
                    }
                }
            }
            lambda_error = (1.0 - sum) / slope;
            lambda = lambda + LAMBDA_STEP_FRACTION * lambda_error;
            iteration_count++;
        }
        *lambdaToReturn = lambda;
        printf("Lambda iteration count %d\n", iteration_count );
        printf("the lambda value = %f \t sum of jp = %12.10f \n", lambda,
               sum);
        re_to_return = 0.0;
        for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
            for (j = 0;  j < COMPOSITION_ALPHABET_SIZE;  j++) {
                if (eRelEntropyOldMatrixNewContext == NRrecord->flag) {
                    re_to_return +=
                        lambda * NRrecord->score_old[i][j] *
                        exp(NRrecord->score_old[i][j] * lambda) *
                        NRrecord->first_seq_freq[i] *
                        NRrecord->second_seq_freq[j];
                } else {
                    if (eUnconstrainedRelEntropy == NRrecord->flag) {
                        re_to_return +=
                            lambda * NRrecord->score_final[i][j] *
                            exp(NRrecord->score_final[i][j] * lambda) *
                            NRrecord->first_seq_freq[i] *
                            NRrecord->second_seq_freq[j];
                    }
                }
            }
        }
    }
    return re_to_return;
}


/**
 * RE_interface is the main function in this code file that provides
 * the connection between blastpgp and the code file kappa.c (from
 * where RE_interface is called) to the optimization procedures
 * matrixname is the underlying 20X20 score matrix; currently only
 * BLOSUM62 is supported
 *
 * - length1 and length2 are the adjusted lengths (not counting X) of
 *    the two sequences
 * - probArray1 and probaArray2 are the the two letter probability
 *   distributions pseudocounts are used later to adjust the probability
 *   distributions
 * - specifiedRE is a fixed relative entropy that might be used in one
 *   optimization constraint
 * - NRrecord stores all field needed for multidimensional Newton's
 *   method lambda is returned*/
int
Blast_AdjustComposition(const char *matrixName,
                        int length1,
                        int length2,
                        const double * probArray1,
                        const double * probArray2,
                        int pseudocounts,
                        double specifiedRE,
                        Blast_CompositionWorkspace * NRrecord,
                        double * lambdaComputed)
{
    int i;                         /* loop indices */
    double re_o_newcontext = 0.0;  /* relative entropy implied by
                                      input single sequence
                                      probabilities */
    static int total_iterations = 0;   /* total iterations among all
                                          calls to
                                          compute_new_score_matrix */
    int new_iterations = 0;        /* number of iterations in the most
                                      recent call to
                                      compute_new_score_matrix */
    static int max_iterations = 0; /* maximum number of iterations
                                      observed in a call to
                                      compute_new_score_matrix */
    int status;                    /* status code for operations that may
                                      fail */
    /*Is the relative entropy constrained? Behaves as boolean for now*/
    int constrain_rel_entropy =
        eUnconstrainedRelEntropy != NRrecord->flag;

    Blast_ApplyPseudocounts(NRrecord->first_seq_freq_wpseudo, 
                            NRrecord->first_seq_freq, probArray1, length1,
                            NRrecord->first_standard_freq, pseudocounts);
    /* plug in frequencies for second sequence, will be the matching
       sequence in BLAST */
    Blast_ApplyPseudocounts(NRrecord->second_seq_freq_wpseudo,
                            NRrecord->second_seq_freq, probArray2, length2,
                            NRrecord->second_standard_freq, pseudocounts);
    *lambdaComputed = 1.0;
    re_o_newcontext =
        Blast_CalcLambdaForComposition(
            NRrecord, (NRrecord->flag == eRelEntropyOldMatrixNewContext),
            lambdaComputed);
    switch (NRrecord->flag) {
    case eUnconstrainedRelEntropy:
        /* Initialize to a arbitrary value; it won't be used */
        NRrecord->RE_final = 0.0;
        break;
    case eRelEntropyOldMatrixNewContext:
        NRrecord->RE_final = re_o_newcontext;
        break;
    case eRelEntropyOldMatrixOldContext:
        NRrecord->RE_final = NRrecord->RE_o_implicit;
        break;
    case eUserSpecifiedRelEntropy:
        NRrecord->RE_final = specifiedRE;
        break;
    default:
        fprintf(stderr, "RE FLAG value of %d is unrecognized\n",
                NRrecord->flag);
        break;
    }
    status =
        Blast_OptimizeTargetFrequencies(&NRrecord->mat_final[0][0],
                                        COMPOSITION_ALPHABET_SIZE,
                                        &new_iterations,
                                        &NRrecord->mat_b[0][0],
                                        NRrecord->first_seq_freq_wpseudo,
                                        NRrecord->second_seq_freq_wpseudo,
                                        constrain_rel_entropy,
                                        NRrecord->RE_final,
                                        kCompoAdjustErrTolerance,
                                        kCompoAdjustIterationLimit);
    total_iterations += new_iterations;
    if (new_iterations > max_iterations)
        max_iterations = new_iterations;

    if (status == 0) {
        Blast_ScoreMatrixFromFreq(NRrecord->score_final,
                                  COMPOSITION_ALPHABET_SIZE,
                                  NRrecord->mat_final,
                                  NRrecord->first_seq_freq_wpseudo,
                                  NRrecord->second_seq_freq_wpseudo);
        if (NRrecord->flag == eUnconstrainedRelEntropy) {
            /* Compute the unconstrained relative entropy */
            double re_free =
                Blast_CalcLambdaForComposition(NRrecord, 1, lambdaComputed);
            printf("RE_uncons  = %6.4f\n\n", re_free);
        }
        status = 0;
    } else if (status == -1) {
        /* Generic error, probably out of memory */
        status = -1;
    } else {
        /* Iteration did not converge */
        fprintf(stderr, "bad probabilities from sequence 1, length %d\n",
                length1);
        for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++)
            fprintf(stderr, "%15.12f\n", probArray1[i]);
        fprintf(stderr, "bad probabilities from sequence 2, length %d\n",
                length2);
        for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++)
            fprintf(stderr, "%15.12f\n", probArray2[i]);
        fflush(stderr);
        status = 1;
    }
    return status;
}
