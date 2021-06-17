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
 * Author:  Jason Papadopoulos
 *
 */

/** @file blast_tune.c
 * Routines that compute a blastn word size appropriate for finding,
 * with high probability, alignments with specified length and 
 * percent identity.
 */

#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_tune.h>

/** structure containing intermediate data to be processed */
typedef struct MatrixData {
    Int4 matrix_dim_alloc;      /**< max matrix size */
    Int4 matrix_dim;            /**< current matrix size */
    double hit_probability;     /**< for the current Markov chain, the
                                     probability that blastn will find
                                     a hit of specified length with 
                                     specified identity */
    double percent_identity;    /**< the target percent identity, used
                                     to choose the blastn word size */
    double *power_matrix;       /**< space for iterated Markov chain */
    double *prod_matrix;        /**< scratch space for matrix multiply */
} MatrixData;

/** the probability that a random alignment will be found.
    Given particulars about the alignment, we will attempt
    to compute the largest blastn word size that has at least
    this probability of finding a random alignment */
#define TARGET_HIT_PROB 0.98

/** initialize intermediate state. Note that memory for
 *  the matrices gets allocated later.
 * @param m pointer to intermediate state [in][out]
 * @return -1 if m is NULL, zero otherwise
 */
static Int2 s_MatrixDataInit(MatrixData *m)
{
    if (m == NULL)
        return -1;

    memset(m, 0, sizeof(MatrixData));
    return 0;
}

/** Free previously allocated scratch data
 * @param m pointer to intermediate state [in][out]
 */
static void s_MatrixDataFree(MatrixData *m)
{
    if (m != NULL) {
        sfree(m->power_matrix);
        sfree(m->prod_matrix);
    }
}

/** Set up for the next calculation of hit probability.
 * @param m Space for the Markov chain calculation [in][out]
 * @param new_word_size The blastn word size to be used
 *              for the current test. The internally generated 
 *              matrix has dimension one larger than this [in]
 * @param percent_identity The desired amount of identity in
 *              alignments. A fractional number (0...1) [in]
 * @return 0 if successful
 */
static Int2 s_MatrixDataReset(MatrixData *m, 
                              Int4 new_word_size, 
                              double percent_identity)
{
    if (m == NULL)
        return -1;

    m->hit_probability = 0.0;
    m->percent_identity = percent_identity;
    m->matrix_dim = new_word_size + 1;

    /* reallocate the two scratch matrices only if the new
       matrix dimension exceeds the amount of space previously
       allocated */
    if (m->matrix_dim > m->matrix_dim_alloc) {

        Int4 num_cells = m->matrix_dim * m->matrix_dim;
        m->matrix_dim_alloc = m->matrix_dim;
        m->power_matrix = (double *)realloc(m->power_matrix, 
                                      num_cells * sizeof(double));
        m->prod_matrix = (double *)realloc(m->prod_matrix, 
                                      num_cells * sizeof(double));

        if (m->power_matrix == NULL || m->prod_matrix == NULL) {
            sfree(m->power_matrix);
            sfree(m->prod_matrix);
            return -2;
        }
    }
    return 0;
}

/** Loads the initial value for matrix exponentiation. This is
 *  the starting Markov chain described in the reference.
 * @param matrix The matrix to be initialized [in][out]
 * @param matrix_dim Dimension of the matrix [in]
 * @param identity The desired amount of identity in
 *              alignments. A fractional number (0...1) [in]
 */
static void s_SetInitialMatrix(double *matrix, 
                               Int4 matrix_dim,
                               double identity)
{
    Int4 i;
    double *row;

    memset(matrix, 0, matrix_dim * matrix_dim * sizeof(double));

    for (i = 0, row = matrix; i < matrix_dim - 1; 
                        i++, row += matrix_dim) {
        row[0] = 1.0 - identity;
        row[i+1] = identity;
    }
    row[i] = 1.0;
}

/** Multiply the current exponentiated matrix by the original
 *  state transition matrix. Since the latter is very sparse and
 *  has a regular structure, this operation is essentially
 *  instantaneous compared to an ordinary matrix-matrix multiply
 * @param a Matrix to multiply [in]
 * @param identity The desired amount of identity in
 *              alignments. A fractional number (0...1). Note that
 *              this is the only information needed to create the
 *              state transition matrix, and its structure is sufficiently
 *              regular that the matrix can be implicitly used [in]
 * @param prod space for the matrix product [out]
 * @param dim The dimension of all matrices [in]
 */
static void s_MatrixMultiply(double *a, 
                             double identity,
                             double *prod, Int4 dim)
{
    Int4 i, j;
    double *prod_row;
    double *a_row;
    double comp_identity = 1.0 - identity;

    /* compute the first column of the product */
    a_row = a;
    prod_row = prod;
    for (i = 0; i < dim; i++) {

        double accum = 0;
        for (j = 0; j < dim - 1; j++)
            accum += a_row[j];

        prod_row[0] = comp_identity * accum;
        a_row += dim;
        prod_row += dim;
    }

    /* computed the second to the last columns */
    a_row = a;
    prod_row = prod;
    for (i = 0; i < dim; i++) {
        for (j = 1; j < dim; j++) {
            prod_row[j] = identity * a_row[j-1];
        }
        a_row += dim;
        prod_row += dim;
    }

    /* modify the last column slightly */
    a_row = a + dim - 1;
    prod_row = prod + dim - 1;
    for (i = 0; i < dim; i++) {
        prod_row[0] += a_row[0];
        a_row += dim;
        prod_row += dim;
    }
}

/** Multiply a square matrix by itself
 * @param a The matrix [in]
 * @param prod Space to store the product [out]
 * @param dim The matrix dimesnion [in]
 */
static void s_MatrixSquare(double *a, double *prod, Int4 dim)
{
    Int4 i, j, k;
    double *prod_row = prod;
    double *a_row = a;
    Int4 full_entries = dim & ~3;

    /* matrix multiplication is probably the most heavily
       studied computational problem, and there are many 
       high-quality implementations for computing matrix 
       products. All of them 1) are enormously faster than 
       this implementation, 2) are far more complicated than
       is practical, 3) are optimized for matrix sizes much
       larger than are dealt with here, and 4) are not worth
       adding a dependency on a BLAS implementation just for
       this application. The following is 'fast enough' */

    for (i = 0; i < dim; i++, prod_row += dim, a_row += dim) {

        for (j = 0; j < dim; j++) {

            double *a_col = a + j;
            double accum = 0;
            for (k = 0; k < full_entries; k += 4, a_col += 4 * dim) {
                accum += a_row[k] * a_col[0] +
                         a_row[k+1] * a_col[dim] +
                         a_row[k+2] * a_col[2*dim] +
                         a_row[k+3] * a_col[3*dim];
            }
            for (; k < dim; k++, a_col += dim) {
                accum += a_row[k] * a_col[0];
            }

            prod_row[j] = accum;
        }
    }
}

/** swap two matrices by swapping pointers to them */
#define SWAP_MATRIX(a,b) {      \
        double *tmp = (a);      \
        (a) = (b);              \
        (b) = tmp;              \
}

/** For fixed word size and alignment properties, compute
 * the probability that blastn with that word size will 
 * find a seed within a random alignment.
 * @param m Space for the Markov chain calculation [in][out]
 * @param word_size The blastn word size [in]
 * @param min_percent_identity How much identity is expected in
 *              random alignments. Less identity means the probability of
 *              finding such alignments is decreased [in]
 * @param min_align_length The smallest alignment length desired.
 *              Longer length gives blastn more leeway to find seeds
 *              and increases the computed probability that alignments
 *              will be found [in]
 * @return 0 if the probability was successfully computed
 */
static Int2 s_FindHitProbability(MatrixData *m, 
                                 Int4 word_size,
                                 double min_percent_identity,
                                 Int4 min_align_length)
{
    Uint4 mask;
    Int4 num_squares = 0;

    if (min_align_length == 0)
        return -3;

    if (s_MatrixDataReset(m, word_size, min_percent_identity))
        return -4;

    /* initialize the matrix of state transitions */
    s_SetInitialMatrix(m->power_matrix, m->matrix_dim,
                       min_percent_identity);

    /* Exponentiate the starting matrix. The probability desired 
       is the top right entry of the resulting matrix. Use left-to-
       right binary exponentiation, since this allows the original
       (very sparse) transition matrix to be used throughout the
       exponentiation process */

    mask = (Uint4)(0x80000000);
    while (!(min_align_length & mask))
        mask = mask / 2;

    for (mask = mask / 2, num_squares = 0; mask; 
                        mask = mask / 2, num_squares++) {

        if (num_squares == 0)
            s_MatrixMultiply(m->power_matrix, m->percent_identity,
                             m->prod_matrix, m->matrix_dim);
        else
            s_MatrixSquare(m->power_matrix, m->prod_matrix, m->matrix_dim);
        SWAP_MATRIX(m->prod_matrix, m->power_matrix);

        if (min_align_length & mask) {
            s_MatrixMultiply(m->power_matrix, m->percent_identity,
                             m->prod_matrix, m->matrix_dim);
            SWAP_MATRIX(m->prod_matrix, m->power_matrix);
        }
    }

    m->hit_probability = m->power_matrix[m->matrix_dim - 1];
    return 0;
}


/** For specified alignment properties, compute the blastn word size
 * that will cause random alignments with those properties to be 
 * found with specified (high) probability.
 * @param m Space for the Markov chain calculation [in][out]
 * @param min_percent_identity How much identity is expected in
 *              random alignments [in]
 * @param min_align_length The smallest alignment length desired [in]
 * @return The optimal word size, or zero if the optimization 
 *         process failed
 */
static Int4 s_FindWordSize(MatrixData *m,
                           double min_percent_identity,
                           Int4 min_align_length)
{
    const double k_min_w = 4;     /* minimum acceptable word size */
    const double k_max_w = 110;     /* maximum acceptable word size */
    double w0, p0;
    double w1, p1;

    /* we treat the optimization problem as an exercise in
       rootfinding, and use bisection. Bisection is appropriate
       here because the root does not need to be found to 
       high accuracy (since the final word size must be an
       integer) and because the function described by
       s_FindHitProbability is monotonically decreasing but
       can drop off very sharply, i.e. can still be badly behaved.

       Begin by bracketing the target probability. The initial range
       should be appropriate for common searches */

    w1 = 28.0;
    if (s_FindHitProbability(m, (Int4)(w1 + 0.5), 
                             min_percent_identity,
                             min_align_length) != 0) {
        return 0;
    }
    p1 = m->hit_probability - TARGET_HIT_PROB;

    w0 = 11.0;
    if (s_FindHitProbability(m, (Int4)(w0 + 0.5), 
                             min_percent_identity,
                             min_align_length) != 0) {
        return 0;
    }
    p0 = m->hit_probability - TARGET_HIT_PROB;

    /* modify the initial range if it does not bracket the
       target probability */
    if (p1 > 0) {

        /* push the range to the right. Progressively double
           the word size until the root is bracketed or the
           maximum word size is reached */

        while (p1 > 0 && w1 < k_max_w) {
            w0 = w1; p0 = p1;
            w1 = MIN(2 * w1, k_max_w);
            if (s_FindHitProbability(m, (Int4)(w1 + 0.5), 
                                    min_percent_identity,
                                    min_align_length) != 0) {
                return 0;
            }
            p1 = m->hit_probability - TARGET_HIT_PROB;
        }

        /* if the root is still not bracketed, return the
           largest possible word size */

        if (p1 > 0)
            return (Int4)(w1 + 0.5);
    }
    else if (p0 < 0) {

        /* push the range to the left. The smallest word size
           is reached much sooner, so choose it immediately */

        w1 = w0; p1 = p0;
        w0 = k_min_w;
        if (s_FindHitProbability(m, (Int4)(w0 + 0.5), 
                                 min_percent_identity,
                                 min_align_length) != 0) {
            return 0;
        }
        p0 = m->hit_probability - TARGET_HIT_PROB;

        /* and return that word size if it's still not enough */
        if (p0 < 0)
            return (Int4)(w0 + 0.5);
    }

    /* bisect the initial range until the bounds have
       converged to each other */
    while (fabs(w1 - w0) > 1) {
        double p2, w2 = (w0 + w1) / 2;

        if (s_FindHitProbability(m, (Int4)(w2 + 0.5), 
                                min_percent_identity,
                                min_align_length) != 0) {
            return 0;
        }
        p2 = m->hit_probability - TARGET_HIT_PROB;

        if (p2 > 0.0) {
            w0 = w2; p0 = p2;
        }
        else {
            w1 = w2; p1 = p2;
        }
    }

    /* conservatively return the lower bound, since that gives
       a more accurate word size */
    return (Int4)(w0 + 0.5);
}

/* see blast_tune.h */
Int4 BLAST_FindBestNucleotideWordSize(double min_percent_identity,
                                      Int4 min_align_length)
{
    MatrixData m;
    Int4 retval;

    /* perform sanity checks */

    if (min_percent_identity >= 1.0 || min_percent_identity < 0.6)
        return 0;

    if (min_align_length > 10000)
        min_align_length = 10000;
    else if (min_align_length < 0)
        return 0;
    else if (min_align_length < 8)
        return 4;

    /* find the best word size */
    s_MatrixDataInit(&m);
    retval = s_FindWordSize(&m, min_percent_identity,
                            min_align_length);
    s_MatrixDataFree(&m);
    return retval;
}
