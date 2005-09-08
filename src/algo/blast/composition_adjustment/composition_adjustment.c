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

/*****************************************************************************

File name: Newton_procedures.c

Authors: Yi-Kuo Yu, Alejandro Schaffer, E. Michael Gertz

Contents: Highest level functions to solve the optimization problem 
          for compositional score matrix adjustment

******************************************************************************/

/* Functions that find the new joint probability matrix given 
   an original joint prob and new set(s) of background frequency(ies)
   The eta parameter is a lagrangian multiplier to
   fix the relative entropy between the new target and the new background

   RE_FLAG is used to indicate various choices as encoded in NRdefs.h
*/

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <algo/blast/core/blast_toolkit.h>
#include <algo/blast/composition_adjustment/composition_adjustment.h>
#include <algo/blast/composition_adjustment/optimize_target_freq.h>
#include <algo/blast/composition_adjustment/nlm_linear_algebra.h>

/*allocate one record of type NRitems, allocate memory for its
  arrays, and return a pointer to the record*/
NRitems *
allocate_NR_memory()
{
    NRitems * NRrecord;        /* record to allocate and return */
    int i;                     /* loop index */

    NRrecord = (NRitems *) malloc(sizeof(NRitems));
    assert(NRrecord != NULL);
    NRrecord->first_standard_freq =
        (double *) malloc(Alphsize * sizeof(double));
    assert(NRrecord->first_standard_freq != NULL);

    NRrecord->second_standard_freq =
        (double *) malloc(Alphsize * sizeof(double));
    assert(NRrecord->second_standard_freq != NULL);

    NRrecord->first_seq_freq =
        (double *) malloc(Alphsize * sizeof(double));
    assert(NRrecord->first_seq_freq != NULL);

    NRrecord->second_seq_freq =
        (double *) malloc(Alphsize * sizeof(double));
    assert(NRrecord->second_seq_freq != NULL);

    NRrecord->first_seq_freq_wpseudo =
        (double *) malloc(Alphsize * sizeof(double));
    assert(NRrecord->first_seq_freq_wpseudo != NULL);

    NRrecord->second_seq_freq_wpseudo =
        (double *) malloc(Alphsize * sizeof(double));
    assert(NRrecord->second_seq_freq_wpseudo != NULL);

    NRrecord->score_old   = Nlm_DenseMatrixNew(Alphsize, Alphsize);
    NRrecord->score_final = Nlm_DenseMatrixNew(Alphsize, Alphsize);
    NRrecord->mat_final   = Nlm_DenseMatrixNew(Alphsize, Alphsize);
    NRrecord->mat_b       = Nlm_DenseMatrixNew(Alphsize, Alphsize);
    for (i = 0;  i < Alphsize;  i++) {
        NRrecord->first_standard_freq[i] = NRrecord->second_standard_freq[i] =
            0.0;
        NRrecord->first_seq_freq[i] = NRrecord->second_seq_freq[i] = 0.0;
        NRrecord->first_seq_freq_wpseudo[i] =
            NRrecord->second_seq_freq_wpseudo[i] = 0.0;
    }
    return NRrecord;
}


/*free memory assoicated with a record of type NRitems*/
void
free_NR_memory(NRitems * NRrecord)
{
    free(NRrecord->first_standard_freq);
    NRrecord->first_standard_freq = NULL;
    free(NRrecord->second_standard_freq);
    NRrecord->second_standard_freq = NULL;

    free(NRrecord->first_seq_freq);
    NRrecord->first_seq_freq  = NULL;
    free(NRrecord->second_seq_freq);
    NRrecord->second_seq_freq = NULL;

    free(NRrecord->first_seq_freq_wpseudo);
    NRrecord->first_seq_freq_wpseudo = NULL;
    free(NRrecord->second_seq_freq_wpseudo);
    NRrecord->second_seq_freq_wpseudo = NULL;

    Nlm_DenseMatrixFree(NRrecord->score_old);
    NRrecord->score_old = NULL;
    Nlm_DenseMatrixFree(NRrecord->score_final);
    NRrecord->score_final = NULL;
    Nlm_DenseMatrixFree(NRrecord->mat_final);
    NRrecord->mat_final = NULL;
    Nlm_DenseMatrixFree(NRrecord->mat_b);
    NRrecord->mat_b = NULL;

    free(NRrecord);
}


/*compute Lambda and if flag set according return re_o_newcontext,
  otherwise return 0.0, also test for the possibility of average
  score >= 0*/
double
compute_lambda(NRitems * NRrecord,
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

    if (RE_OLDMAT_NEWCONTEXT == NRrecord->flag) {
        ave_score = 0.0;
        for (i = 0;  i < Alphsize;  i++) {
            for (j = 0;  j < Alphsize;  j++) {
                ave_score +=
                    NRrecord->score_old[i][j] * NRrecord->first_seq_freq[i] *
                    NRrecord->second_seq_freq[j];
            }
        }
    }
    if ((RE_OLDMAT_NEWCONTEXT == NRrecord->flag) &&
        (ave_score >= (-SCORE_BOUND))) {
        /* fall back to no constraint mode when ave score becomes global
           alignment-like */
        NRrecord->flag = RE_NO_CONSTRAINT;

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
            for (i = 0;  i < Alphsize;  i++) {
                for (j = 0;  j < Alphsize;  j++) {
                    if (RE_OLDMAT_NEWCONTEXT == NRrecord->flag) {
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
            for (i = 0;  i < Alphsize;  i++) {
                for (j = 0;  j < Alphsize;  j++) {
                    if (RE_OLDMAT_NEWCONTEXT == NRrecord->flag) {
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
                        if(RE_NO_CONSTRAINT == NRrecord->flag) {
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
        printf("the lambda value = %lf \t sum of jp = %12.10f \n", lambda,
               sum);
        re_to_return = 0.0;
        for (i = 0;  i < Alphsize;  i++) {
            for (j = 0;  j < Alphsize;  j++) {
                if (RE_OLDMAT_NEWCONTEXT == NRrecord->flag) {
                    re_to_return +=
                        lambda * NRrecord->score_old[i][j] *
                        exp(NRrecord->score_old[i][j] * lambda) *
                        NRrecord->first_seq_freq[i] *
                        NRrecord->second_seq_freq[j];
                } else {
                    if (RE_NO_CONSTRAINT == NRrecord->flag) {
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


void
Blast_ScoreMatrixFromFreq(double ** score, int alphsize, double ** freq,
                          const double row_sum[], const double col_sum[])
{
    int i, j;
    double sum;

    sum = 0.0;
    for (i = 0;  i < alphsize;  i++) {
        for (j = 0; j < alphsize;  j++) {
            sum +=  freq[i][j];
        }
    }
    for (i = 0;  i < alphsize;  i++) {
        for (j = 0; j < alphsize;  j++) {
            score[i][j] = log(freq[i][j] / sum / row_sum[i] / col_sum[j]);
        }
    }
}


/** This procedures helps set up the direct solution of the new score
 * matrix by a Newtonian procedure and converts the results into a
 * score matrix. This is the highest level procedure shared by the
 * code as it is used both inside and outside BLAST.  NRrecord keeps
 * track of the variabales used in the Newtonian optimization; tol is
 * the tolerence to be used to declare convergence to a local optimum;
 * maxits is the maximum number of iterations allowed*/

int
score_matrix_direct_solve(NRitems * NRrecord,
                          double tol,
                          int maxits)
{
    int its;    /* number of iterations used*/

    /*Is the relative entropy constrained? Behaves as boolean for now*/
    int constrain_rel_entropy =
        RE_NO_CONSTRAINT != NRrecord->flag;

    its =
        Blast_OptimizeTargetFrequencies(&NRrecord->mat_final[0][0],
                                        Alphsize,
                                        &NRrecord->mat_b[0][0],
                                        NRrecord->first_seq_freq_wpseudo,
                                        NRrecord->second_seq_freq_wpseudo,
                                        constrain_rel_entropy,
                                        NRrecord->RE_final,
                                        tol, maxits);

    if (its <= maxits) {
        Blast_ScoreMatrixFromFreq(NRrecord->score_final, Alphsize,
                                  NRrecord->mat_final,
                                  NRrecord->first_seq_freq_wpseudo,
                                  NRrecord->second_seq_freq_wpseudo);
    }
    return its;
}


/*compute the symmetric form of the relative entropy of two
 *probability vectors 
 *In this software relative entropy is expressed in "nats", 
 *meaning that logarithms are base e. In some other scientific 
 *and engineering domains where entropy is used, the logarithms 
 *are taken base 2 and the entropy is expressed in bits.
*/
double
Blast_GetRelativeEntropy(double * A, double * B)
{
    int i;                 /* loop index over letters */
    double temp;           /* intermediate term */
    double value = 0.0;    /* square of relative entropy */

    for (i = 0;  i < Alphsize;  i++) {
        temp = (A[i] + B[i]) / 2;
        if (temp > 0) {
            if (A[i] > 0) {
                value += A[i] * log(A[i] / temp) / 2;
            }
            if (B[i] > 0) {
                value += B[i] * log(B[i] / temp) / 2;
            }
        }
    }
    if (value < 0) {             /* must be numerical rounding error */
        value = 0;
    }
    return sqrt(value);
}
