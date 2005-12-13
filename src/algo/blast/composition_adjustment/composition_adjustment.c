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

/** @file composition_adjustment.c
 *
 * @author Yi-Kuo Yu, Alejandro Schaffer, E. Michael Gertz
 *
 * Highest level functions to solve the optimization problem for
 * compositional score matrix adjustment.
 */
#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <limits.h>
#include <assert.h>
#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/composition_adjustment/composition_constants.h>
#include <algo/blast/composition_adjustment/composition_adjustment.h>
#include <algo/blast/composition_adjustment/matrix_frequency_data.h>
#include <algo/blast/composition_adjustment/nlm_linear_algebra.h>
#include <algo/blast/composition_adjustment/optimize_target_freq.h>

/**positions of true characters in protein alphabet*/
static int trueCharPositions[COMPO_NUM_TRUE_AA] =
{1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,22};

/**
 * conversion from 26 letter NCBIstdaa alphabet to 20 letter order
 * for true amino acids: ARNDCQEGHILKMFPSTWYV.  This order is
 * alphabetical in the standard three-letter abbreviation of each
 * amino acid */
static int alphaConvert[COMPO_PROTEIN_ALPHABET] =
  {(-1), 0, (-1),  4, 3, 6, 13, 7, 8, 9, 11, 10, 12, 2, 14, 5, 1, 15,
   16, 19, 17, (-1), 18, (-1), (-1), (-1)};


/**
 * Desired margin between an end of region used for computing a
 * composition, and the nearest StopChar; the desired margin may
 * not be attained. */
static const int kCompositionMargin = 20;

#define SCORE_BOUND            0.0000000001 /**< average scores below 
                                                 -SCORE_BOUND are considered
                                                 effectively nonnegative, and
                                                 Newton's method will
                                                 will terminate */
#define LAMBDA_STEP_FRACTION   0.5          /**< default step fraction in
                                                 Newton's method */
#define INITIAL_LAMBDA         1.0          /**< initial value for Newton's
                                                 method */
#define LAMBDA_ITERATION_LIMIT 300          /**< iteration limit for Newton's
                                                 method. */
#define LAMBDA_ERROR_TOLERANCE 0.0000001    /**< bound on error for estimating
                                                 lambda */

/** bound on error for Newton's method */
static const double kCompoAdjustErrTolerance = 0.00000001;
/** iteration limit for Newton's method */
static const int kCompoAdjustIterationLimit = 2000;
/** relative entropy of BLOSUM62 */
static const double kFixedReBlosum62 = 0.44;

/**
 * Find the weighted average of a set of observed probabilities with a
 * set of "background" probabilities.  All array parameters have
 * length COMPO_NUM_TRUE_AA.
 *
 * @param probs_with_pseudo       an array of weighted averages [out]
 * @param normalized_probs        observed frequencies, normalized to sum
 *                                to 1.0 [out]
 * @param observed_freq           observed frequencies, not necessarily
 *                                normalized to sum to 1.0. [in]
 * @param background_probs        the probability of characters in a
 *                                standard sequence.
 * @param number_of_observations  the number of characters used to
 *                                form the observed_freq array
 * @param pseudocounts            the number of "standard" characters
 *                                to be added to form the weighted
 *                                average.
 */
static void
Blast_ApplyPseudocounts(double * probs_with_pseudo,
                        double * normalized_probs,
                        const double * observed_freq,
                        int number_of_observations,
                        const double * background_probs,
                        int pseudocounts)
{
    int i;                 /* loop index */
    double weight;         /* weight assigned to pseudocounts */
    double sum;            /* sum of the observed frequencies */
    double dpseudocounts; /* pseudocounts as a double */

    dpseudocounts = pseudocounts;

    /* Normalize probabilities */
    sum = 0.0;
    for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
        sum += observed_freq[i];
    }
    if (sum > 0) {
        for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
            normalized_probs[i] = observed_freq[i]/sum;
        }
    }
    weight = dpseudocounts / (number_of_observations + dpseudocounts);
    for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
        probs_with_pseudo[i] =
            (1.0 - weight) * normalized_probs[i] +
            weight * background_probs[i];
    }
}


/**
 * Create a score matrix from a set of target frequencies.  The scores
 * are scaled so that the Karlin-Altschul statistical parameter Lambda
 * equals (within numerical precision) 1.0.
 *
 * @param score        the new score matrix [out]
 * @param alphsize     the number of rows and columns of score
 * @param freq         a matrix of target frequencies [in]
 * @param row_sum      sum of each row of freq [in]
 * @param col_sum      sum of each column of freq[in]
 */
static void
Blast_ScoreMatrixFromFreq(double ** score, int alphsize, double ** freq,
                          const double row_sum[], const double col_sum[])
{
    int i, j;          /* array indices */
    double sum;        /* sum of values in freq; used to normalize freq */

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


/**
 * Compute the symmetric form of the relative entropy of two
 * probability vectors
 *
 * In this software relative entropy is expressed in "nats",
 * meaning that logarithms are base e. In some other scientific
 * and engineering domains where entropy is used, the logarithms
 * are taken base 2 and the entropy is expressed in bits.
 *
 * @param A    an array of length COMPO_NUM_TRUE_AA of
 *             probabilities.
 * @param B    a second array of length COMPO_NUM_TRUE_AA of
 *             probabilities.
 */
double
Blast_GetRelativeEntropy(const double A[], const double B[])
{
    int i;                 /* loop index over letters */
    double temp;           /* intermediate term */
    double value = 0.0;    /* square of relative entropy */

    for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
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


/**
 * Convert letter probabilities from a 26-letter NCBIstdaa alphabet to
 * a 20 letter ARND... amino acid alphabet. (@see alphaConvert)
 *
 * @param inputLetterProbs    the 26-letter probabilities [in]
 * @param outputLetterProbs   the 20-letter probabilities [out]
 */
static void
s_GatherLetterProbs(double * outputLetterProbs,
                    const double * inputLetterProbs)
{
    int c; /*index over characters*/

    for (c = 0;  c < COMPO_PROTEIN_ALPHABET;  c++) {
        if ((-1) != alphaConvert[c]) {
            outputLetterProbs[alphaConvert[c]] = inputLetterProbs[c];
        }
    }
}


/**
 * Scatter and scale a matrix of scores for a 20 letter ARND...  amino
 * acid alphabet into a matrix for a 26 letter NCBIstdaa alphabet
 * (@see alphaConvert), leaving scores for any character not present
 * in the smaller alphabet untouched.
 *
 * @param dMatrix         frequency ratios for the 26 letter alphabet [out]
 * @param dMatrixTrueAA   frequency ratios for the 20 letter alphabet [in]
 * @param scale           multiply the elements in dMatrixTrueAA by
 *                        scale when applying the scatter.
 */
static void
s_ScatterScores(double ** dMatrix,
                double scale,
                double ** dMatrixTrueAA)
{
    int p, c; /*indices over positions and characters*/

    for (p = 0;  p < COMPO_PROTEIN_ALPHABET;  p++) {
        for (c = 0;  c < COMPO_PROTEIN_ALPHABET;  c++) {
            if (((-1) != alphaConvert[p]) && ((-1) != alphaConvert[c])) {
                dMatrix[p][c] =
                    scale * dMatrixTrueAA[alphaConvert[p]][alphaConvert[c]];
            }
        }
    }
}


/**
 * Average the scores for two characters to get scores for an
 * ambiguity character than represents either of the two original
 * characters.
 *
 * @param dMatrix    score matrix -- on entry contains the score data
 *                   for characters A and B, and on exit also contains
 *                   the score data for ambigAB.
 * @param A          a character in the alphabet
 * @param B          another character in the alphabet
 * @param ambigAB    the combined ambiguity character
 */
static void
Blast_AverageScores(double ** dMatrix, int A, int B, int ambigAB)
{
    int i;           /* iteration index */
    double sum;      /* sum of scores */

    for (i = 0;  i < COMPO_PROTEIN_ALPHABET;  i++) {
        if (-1 != alphaConvert[i]) {
            sum = dMatrix[i][A] + dMatrix[i][B];
            dMatrix[i][ambigAB] = sum/2.0;
            sum = dMatrix[A][i] + dMatrix[B][i];
            dMatrix[ambigAB][i] = sum/2.0;
        }
    }
    /* Because ambiguity characters are rare, we assume a match of
     * ambiguity characters represents a match of the true residues, and
     * so only include matches when computing the average score. */
    sum = dMatrix[A][A] + dMatrix[B][B];
    dMatrix[ambigAB][ambigAB] = sum/2.0;
}


/**
 * Set scores for substitutions that involve the nonstandard amino
 * acids in the NCBIstdaa alphabet: the ambiguity characters 'B' and
 * 'Z'; the "don't care" character 'X'; the atypical amino acid 'U'
 * (Selenocysteine); the stop codon '*'; and the gap or end of
 * sequence character '-'.
 *
 * @param dMatrix      a matrix that on entry contains scores for all the
 *                     true amino acids, and on exit also contains the
 *                     scores for the nonstandard amino acids.
 * @param startMatrix  rounded amino acid substitution scores in
 *                     standard context [in]
 */
static void
Blast_SetNonstandardAaScores(double **dMatrix, int **startMatrix)
{
    int i; /* loop index */
    /* An array containing those special characters whose score will be
       set using startFreqRatios */
    int specialChars[4] =
    { eGapChar,  eXchar, eSelenocysteine, eStopChar };

    /* Set the scores for ambiguity characters B and Z */
    Blast_AverageScores(dMatrix, eDchar, eNchar, eBchar);
    Blast_AverageScores(dMatrix, eEchar, eQchar, eZchar);

    /* (B,Z) mismatches are so rare we simply set their score to zero. */
    dMatrix[eBchar][eZchar] = dMatrix[eZchar][eBchar] = 0.0;

    /* Set the other characters using the startMatrix */
    for (i = 0;  i < 4;  i++) {
        int A, B;     /* Two characters in the alphabet */
        A = specialChars[i];
        for (B = 0;  B < COMPO_PROTEIN_ALPHABET;  B++) {
            dMatrix[A][B] = startMatrix[A][B];
            dMatrix[B][A] = startMatrix[B][A];
        }
    }
}


/** Return the nearest integer to x. */
static long Nint(double x)
{
    x += (x >= 0. ? 0.5 : -0.5);
    return (long)x;
}


/**
 * Round a matrix of floating point scores.
 *
 * @param matrix             the matrix of integer valued scores [out]
 * @param floatScoreMatrix   the matrix of floating point valued
 *                           scores [in]
 * @param numPositions       the number of rows of the matrices.
 */
static void
s_RoundScoreMatrix(int **matrix, double **floatScoreMatrix,
                   int numPositions)
{
    int p, c; /*indices over positions and characters*/

    for (p = 0;  p < numPositions;  p++) {
        for (c = 0;  c < COMPO_PROTEIN_ALPHABET;  c++) {
            if (floatScoreMatrix[p][c] < INT_MIN) {
                matrix[p][c] = INT_MIN;
            } else {
                matrix[p][c] = Nint(floatScoreMatrix[p][c]);
            }
        }
    }
}


/**
 * Find the range of scores contained in an scoring matrix.
 * @param obs_min    smallest value in the matrix
 * @param obs_max    largest value in the matrix
 * @param matrix     a matrix with COMPO_NUM_TRUE_AA columns
 * @param rows       number of rows in the matrix
 */
static void s_GetScoreRange(int * obs_min, int * obs_max,
                            int ** matrix, int rows)
{
    int aa;                    /* index of an amino-acid in the 20
                                  letter alphabet */
    int irow, jcol;            /* matrix row and column indices */
    int minScore, maxScore;    /* largest and smallest observed scores */

    minScore = maxScore = 0;
    for (irow = 0;  irow < rows;  irow++) {
        for (aa = 0;  aa < COMPO_NUM_TRUE_AA;  aa++) {
            jcol = trueCharPositions[aa];
            if (matrix[irow][jcol] < minScore &&
                matrix[irow][jcol] > COMPO_SCORE_MIN)
                minScore = matrix[irow][jcol];
            if (matrix[irow][jcol] > maxScore)
                maxScore = matrix[irow][jcol];
        }
    }
    *obs_min = minScore;
    *obs_max = maxScore;
}


/**
 * Compute the score probabilities for a given amino acid substitution matrix
 * in the context of given query and subject amino acid frequencies.
 *
 * @param *obs_min          the smallest score in the score matrix [out]
 * @param *obs_max          the largest score in the score matrix [out]
 * @param *scoreProb        the new array, of length (*obs_max - *obs_min + 1),
 *                          of score probabilities, where (*scoreProb)[0] is
 *                          the probability for score *obs_min.
 * @param matrix            a amino-acid substitution matrix (not
 *                          position-specific)
 * @param subjectProbArray  is an array containing the probability of
 *                          occurrence of each residue in the subject
 * @param queryProbArray    is an array containing the probability of
 *                          occurrence of each residue in the query
 * @param scoreProb         is an array of probabilities for each score
 *                          that is to be used as a field in return_sfp
 * @return 0 on success, -1 on out-of-memory
 */
static int
s_GetMatrixScoreProbs(double **scoreProb, int * obs_min, int * obs_max,
                            int **matrix, const double *subjectProbArray,
                            const double *queryProbArray)
{
    int aa;          /* index of an amino-acid in the 20 letter
                        alphabet */
    int irow, jcol;  /* matrix row and column indices */
    double * sprob;  /* a pointer to the element of the score
                        probabilities array that represents the
                        probability of the score 0*/
    int minScore;    /* smallest score in matrix; the same value as
                        (*obs_min). */
    int range;       /* the range of scores in the matrix */

    s_GetScoreRange(obs_min, obs_max, matrix, COMPO_PROTEIN_ALPHABET);
    minScore = *obs_min;
    range = *obs_max - *obs_min + 1;
    *scoreProb = calloc(range, sizeof(double));
    if (*scoreProb == NULL) {
        return -1;
    }
    sprob = &((*scoreProb)[-(*obs_min)]); /*center around 0*/
    for (irow = 0;  irow < COMPO_PROTEIN_ALPHABET;  irow++) {
        for (aa = 0;  aa < COMPO_NUM_TRUE_AA;  aa++) {
            jcol = trueCharPositions[aa];
            if (matrix[irow][jcol] >= minScore) {
                sprob[matrix[irow][jcol]] +=
                    (queryProbArray[irow] * subjectProbArray[jcol]);
            }
        }
    }
    return 0;
}


/**
 * Compute the score probabilities for a given amino acid position-specific
 * substitution matrix in the context of a given set of subject amino
 * acid frequencies.
 *
 * @param *obs_min          the smallest score in the score matrix [out]
 * @param *obs_max          the largest score in the score matrix [out]
 * @param *scoreProb        the new array, of length (*obs_max - *obs_min + 1),
 *                          of score probabilities, where (*scoreProb)[0] is
 *                          the probability for score *obs_min.
 * @param matrix            a position-specific amino-acid substitution matrix.
 * @param rows              the number of rows in matrix.
 * @param subjectProbArray  is an array containing the probability of
 *                          occurrence of each residue in the subject
 * @return 0 on success, -1 on out-of-memory
 */
static int
s_GetPssmScoreProbs(double ** scoreProb, int * obs_min, int * obs_max,
                    int **matrix, int rows,
                    const double *subjectProbArray)
{
    int aa;            /* index of an amino-acid in the 20 letter
                          alphabet */
    int irow, jcol;    /* matrix row and column indices */
    double onePosFrac; /* matrix length as a double*/
    double * sprob;    /* pointer to the element of the score
                        * probabilities array the represents the
                        * probability of zero */
    int minScore;      /* smallest score in matrix; the same value as
                          (*obs_min). */
    int range;         /* the range of scores in the matrix */

    s_GetScoreRange(obs_min, obs_max, matrix, rows);
    minScore = *obs_min;
    range = *obs_max - *obs_min + 1;
    *scoreProb = calloc(range, sizeof(double));
    if (*scoreProb == NULL) {
        return -1;
    }
    sprob = &((*scoreProb)[-(*obs_min)]); /*center around 0*/
    onePosFrac = 1.0/ ((double) rows);
    for (irow = 0;  irow < rows;  irow++) {
        for (aa = 0;  aa < COMPO_NUM_TRUE_AA;  aa++) {
            jcol = trueCharPositions[aa];
            if (matrix[irow][jcol] >= minScore) {
                sprob[matrix[irow][jcol]] +=
                    onePosFrac * subjectProbArray[jcol];
            }
        }
    }
    return 0;
}


/**
 * Compute an integer-valued amino-acid score matrix from a set of
 * score frequencies.
 *
 * @param matrix       the preallocated matrix
 * @param alphsize     the size of the alphabet for this matrix
 * @param freq         a set of score frequencies
 * @param Lambda       the desired scale of the matrix
 */
void
Blast_Int4MatrixFromFreq(Int4 **matrix, int alphsize,
                         double ** freq, double Lambda)
{
    int i,j; /*loop indices*/

    for (i = 0;  i < alphsize;  i++) {
        for (j = 0;  j < alphsize;  j++) {
            if (0.0 == freq[i][j]) {
                matrix[i][j] = COMPO_SCORE_MIN;
            } else {
                double temp = log(freq[i][j])/Lambda;
                matrix[i][j] = Nint(temp);
            }
        }
    }
}


/**
 * Fill in one row of a score matrix; used by the s_ScaleMatrix
 * routine to fill in all rows. (@sa s_ScaleMatrix)
 *
 * @param matrixRow      a row of the matrix to be filled in [out].
 * @param startMatrixRow a row of rounded amino acid substitution scores in
 *                       standard context [in]
 * @param freqRatiosRow  a row of frequency ratios of starting matrix [in]
 * @param Lambda         a Karlin-Altschul parameter. [in]
 * @param LambdaRatio    ratio of correct Lambda to it's original value [in]
 */
static void
s_ScaleMatrixRow(int *matrixRow, const int *startMatrixRow,
                 const double *freqRatiosRow,
                 double Lambda, double LambdaRatio)
{
    int c;              /* column index */
    double temp;        /* intermediate term in computation*/

    for (c = 0;  c < COMPO_PROTEIN_ALPHABET;  c++) {
        switch (c) {
        case eGapChar: case eXchar: case eSelenocysteine: case eStopChar:
            /* Don't scale these nonstandard residues */
            matrixRow[c] = startMatrixRow[c];
            break;

        default:
            if (0.0 == freqRatiosRow[c]) {
                matrixRow[c] = COMPO_SCORE_MIN;
            } else {
                temp = log(freqRatiosRow[c]);
                temp = temp/Lambda;
                temp = temp * LambdaRatio;
                matrixRow[c] = Nint(temp);
            } /* end else 0.0 != freqRatiosRow[c] */
        } /* end switch(c) */
    } /* end for c */
}


/** Free memory associated with a Blast_MatrixInfo object */
void Blast_MatrixInfoFree(Blast_MatrixInfo ** ss)
{
    if (*ss != NULL) {
        free((*ss)->matrixName);
        Nlm_Int4MatrixFree(&(*ss)->startMatrix);
        Nlm_DenseMatrixFree(&(*ss)->startFreqRatios);
        free(*ss);
        *ss = NULL;
    }
}


/** Create a Blast_MatrixInfo object
 *
 *  @param rows        the number of rows in the matrix, should be
 *                     COMPO_PROTEIN_ALPHABET unless the matrix is position
 *                     based, in which case it is the query length
 *  @param positionBased  is this matrix position-based?
 */
Blast_MatrixInfo *
Blast_MatrixInfoNew(int rows, int positionBased)
{
    int i;       /* loop index */
    Blast_MatrixInfo * ss = malloc(sizeof(Blast_MatrixInfo));
    if (ss != NULL) {
        ss->rows = rows;
        ss->positionBased = positionBased;

        ss->matrixName = NULL;
        ss->startMatrix = NULL;
        ss->startFreqRatios = NULL;

        ss->startMatrix  = Nlm_Int4MatrixNew(rows + 1, COMPO_PROTEIN_ALPHABET);
        if (ss->startMatrix == NULL)
            goto error_return;
        ss->startFreqRatios = Nlm_DenseMatrixNew(rows + 1, COMPO_PROTEIN_ALPHABET);
        if (ss->startFreqRatios == NULL)
            goto error_return;
        for (i = 0;  i < COMPO_PROTEIN_ALPHABET;  i++) {
            ss->startMatrix[rows][i] = COMPO_SCORE_MIN;
            ss->startFreqRatios[rows][i] = (double) COMPO_SCORE_MIN;
        }

    }
    goto normal_return;
error_return:
    Blast_MatrixInfoFree(&ss);
normal_return:
    return ss;
}


/**
 * Fill in the entries of a score matrix with compositionally adjusted
 * values.  (@sa Blast_CompositionBasedStats)
 *
 * @param matrix       preallocated matrix to be filled in [out]
 * @param ss           data used to compute matrix scores
 * @param LambdaRatio  ratio of correct Lambda to its value in
 *                     standard context.
 */
static void
s_ScaleMatrix(int **matrix, const Blast_MatrixInfo * ss,
              double LambdaRatio)
{
    int p;          /* index over matrix rows */

    if (ss->positionBased) {
        /* scale the matrix rows unconditionally */
        for (p = 0;  p < ss->rows;  p++) {
            s_ScaleMatrixRow(matrix[p], ss->startMatrix[p],
                             ss->startFreqRatios[p],
                             ss->ungappedLambda, LambdaRatio);
        }
    } else {
        /* Scale only the rows for true amino acids and ambiguous residues
         * B and Z. */
        for (p = 0;  p < COMPO_PROTEIN_ALPHABET;  p++) {
            switch (p) {
            case eGapChar: case eXchar: case eSelenocysteine: case eStopChar:
                /* Do not scale the scores of nonstandard amino acids. */
                memcpy(matrix[p], ss->startMatrix[p],
                       COMPO_PROTEIN_ALPHABET * sizeof(int));
                break;
            default:
                s_ScaleMatrixRow(matrix[p], ss->startMatrix[p],
                                 ss->startFreqRatios[p],
                                 ss->ungappedLambda, LambdaRatio);
            }
        }
    }
}


/** LambdaRatioLowerBound is used when the expected score is too large
 * causing impalaKarlinLambdaNR to give a Lambda estimate that
 * is too small, or to fail entirely returning -1*/
#define LambdaRatioLowerBound 0.5


/**
 * Use composition-based statistics to adjust the scoring matrix, as
 * described in
 *
 *     Schaffer, A.A., Aravaind, L., Madden, T.L., Shavirin, S.,
 *     Spouge, J.L., Wolf, Y.I., Koonin, E.V., and Altschul, S.F.
 *     (2001), "Improving the accuracy of PSI-BLAST protein database
 *     searches with composition-based statistics and other
 *     refinements",  Nucleic Acids Res. 29:2994-3005.
 *
 * @param matrix          a scoring matrix to be adjusted [out]
 * @param *LambdaRatio    the ratio of the corrected lambda to the
 *                        original lambda [out]
 * @param ss              data used to compute matrix scores
 *
 * @param queryProb       amino acid probabilities in the query
 * @param resProb         amino acid probabilities in the subject
 * @param calc_lambda     a function that can calculate the
 *                        statistical parameter Lambda from a set of
 *                        score frequencies.
 * @return 0 on success, -1 on out of memory
 */
int
Blast_CompositionBasedStats(int ** matrix, double * LambdaRatio,
                            const Blast_MatrixInfo * ss,
                            const double queryProb[], const double resProb[],
                            double (*calc_lambda)(double*,int,int,double))
{
    double correctUngappedLambda; /* new value of ungapped lambda */
    int obs_min, obs_max;
    double *scoreArray;
    int out_of_memory;
    
    if (ss->positionBased) {
        out_of_memory =
            s_GetPssmScoreProbs(&scoreArray, &obs_min, &obs_max,
                                ss->startMatrix, ss->rows, resProb);
    } else {
        out_of_memory =
            s_GetMatrixScoreProbs(&scoreArray, &obs_min, &obs_max,
                                  ss->startMatrix, resProb, queryProb);
    }
    if (out_of_memory) 
        return -1;
    correctUngappedLambda =
        calc_lambda(scoreArray, obs_min, obs_max, ss->ungappedLambda);

    /* calc_lambda will return -1 in the case where the
     * expected score is >=0; however, because of the MAX statement 3
     * lines below, LambdaRatio should always be > 0; the succeeding
     * test is retained as a vestige, in case one wishes to remove the
     * MAX statement and allow LambdaRatio to take on the error value
     * -1 */
    *LambdaRatio = correctUngappedLambda / ss->ungappedLambda;
    *LambdaRatio = MIN(1, *LambdaRatio);
    *LambdaRatio = MAX(*LambdaRatio, LambdaRatioLowerBound);

    if (*LambdaRatio > 0) {
        s_ScaleMatrix(matrix, ss, *LambdaRatio);
    }
    free(scoreArray);

    return 0;
}


/**
 * Compute the amino acid composition of a sequence.
 *
 * @param composition      the computed composition
 * @param sequence         a sequence of amino acids
 * @param length           length of the sequence
 */
void
Blast_ReadAaComposition(Blast_AminoAcidComposition * composition,
                        const Uint1 * sequence, int length)
{
    int frequency[COMPO_PROTEIN_ALPHABET]; /*frequency of each letter*/
    int i; /*index*/
    int localLength; /*reduce for X characters*/
    double * resProb = composition->prob;

    localLength = length;
    for (i = 0;  i < COMPO_PROTEIN_ALPHABET;  i++)
        frequency[i] = 0;
    for (i = 0;  i < length;  i++) {
        if (eXchar != sequence[i])
            frequency[sequence[i]]++;
        else
            localLength--;
    }
    for (i = 0;  i < COMPO_PROTEIN_ALPHABET;  i++) {
        if (frequency[i] == 0)
            resProb[i] = 0.0;
        else {
            double freq = frequency[i];
            resProb[i] = freq / (double) localLength;
        }
    }
    composition->numTrueAminoAcids = localLength;
}


/**
 * Get the range of a sequence to be included when computing a
 * composition.  This function is used for translated sequences, where
 * the range to use when computing a composition is not the whole
 * sequence, but is rather a range about an existing alignment.
 *
 * @param *pleft, *pright  left and right endpoint of the range
 * @param subject_data     data from a translated sequence
 * @param length           length of subject_data
 * @param start, finish    start and finish (one past the end) of a
 *                         existing alignment
 */
void
Blast_GetCompositionRange(int * pleft, int * pright,
                          const Uint1 * subject_data, int length,
                          int start, int finish)
{
    int i;                /* iteration index */
    int left, right;

    left = start;
    /* Search leftward for a StopChar */
    for (i = left;  i > 0;  i--) {
        if (subject_data[i - 1] == eStopChar) {
            /* We have found a StopChar. Unless the StopChar is
             * too close to the start of the subject region of the
             * HSP, */
            if (i + kCompositionMargin < left) {
                /* reset the left endpoint. */
                left = i + kCompositionMargin;
            }
            break;
        }
    }
    if (i == 0) {
        /* No stop codon was found to the left. */
        left = 0;
    }
    right = finish;
    /* Search rightward for a StopChar */
    for (i = right;  i < length;  i++) {
        if (subject_data[i] == eStopChar) {
            /* We have found a StopChar. Unless the StopChar is
             * too close to the end of the subject region of the
             * HSP, */
            if (i - kCompositionMargin > right) {
                /* reset the right endpoint */
                right = i - kCompositionMargin;
            }
            break;
        }
    }
    if (i == length) {
        /* No stop codon was found to the right. */
        right = length;
    }
    *pleft = left; *pright = right;
}


/** Free memory associated with a record of type
 * Blast_CompositionWorkspace. */
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


/** Create a new Blast_CompositionWorkspace object, allocating memory
 * for all its component arrays. */
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
        (double *) malloc(COMPO_NUM_TRUE_AA * sizeof(double));
    if (NRrecord->first_standard_freq == NULL) goto error_return;

    NRrecord->second_standard_freq =
        (double *) malloc(COMPO_NUM_TRUE_AA * sizeof(double));
    if (NRrecord->second_standard_freq == NULL) goto error_return;

    NRrecord->first_seq_freq =
        (double *) malloc(COMPO_NUM_TRUE_AA * sizeof(double));
    if (NRrecord->first_seq_freq == NULL) goto error_return;

    NRrecord->second_seq_freq =
        (double *) malloc(COMPO_NUM_TRUE_AA * sizeof(double));
    if (NRrecord->second_seq_freq == NULL) goto error_return;

    NRrecord->first_seq_freq_wpseudo =
        (double *) malloc(COMPO_NUM_TRUE_AA * sizeof(double));
    if (NRrecord->first_seq_freq_wpseudo == NULL) goto error_return;

    NRrecord->second_seq_freq_wpseudo =
        (double *) malloc(COMPO_NUM_TRUE_AA * sizeof(double));
    if (NRrecord->second_seq_freq_wpseudo == NULL) goto error_return;

    NRrecord->score_old   = Nlm_DenseMatrixNew(COMPO_NUM_TRUE_AA,
                                               COMPO_NUM_TRUE_AA);
    if (NRrecord->score_old == NULL) goto error_return;

    NRrecord->score_final = Nlm_DenseMatrixNew(COMPO_NUM_TRUE_AA,
                                               COMPO_NUM_TRUE_AA);
    if (NRrecord->score_final == NULL) goto error_return;

    NRrecord->mat_final   = Nlm_DenseMatrixNew(COMPO_NUM_TRUE_AA,
                                               COMPO_NUM_TRUE_AA);
    if (NRrecord->mat_final == NULL) goto error_return;

    NRrecord->mat_b       = Nlm_DenseMatrixNew(COMPO_NUM_TRUE_AA,
                                               COMPO_NUM_TRUE_AA);
    if (NRrecord->mat_b == NULL) goto error_return;

    for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
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


/** Initialize the fields of a Blast_CompositionWorkspace for a specific
 * underlying scoring matrix. */
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
        for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
            for (j = 0;  j < COMPO_NUM_TRUE_AA;  j++) {
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
        for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
            for (j = 0;  j < COMPO_NUM_TRUE_AA;  j++) {
                ave_score +=
                    NRrecord->score_old[i][j] * NRrecord->first_seq_freq[i] *
                    NRrecord->second_seq_freq[j];
            }
        }
    }
    if ((eRelEntropyOldMatrixNewContext == NRrecord->flag) &&
        (ave_score >= (-SCORE_BOUND))) {
        /* fall back to no constraint mode when average score becomes
           global alignment-like */
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
            for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
                for (j = 0;  j < COMPO_NUM_TRUE_AA;  j++) {
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
            for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
                for (j = 0;  j < COMPO_NUM_TRUE_AA;  j++) {
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
        for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
            for (j = 0;  j < COMPO_NUM_TRUE_AA;  j++) {
                if (eRelEntropyOldMatrixNewContext == NRrecord->flag) {
                    double scaledScore = lambda * NRrecord->score_old[i][j];
                    re_to_return += scaledScore * exp(scaledScore) *
                        NRrecord->first_seq_freq[i] *
                        NRrecord->second_seq_freq[j];
                } else {
                    if (eUnconstrainedRelEntropy == NRrecord->flag) {
                        double scaledScore =
                            lambda * NRrecord->score_final[i][j];

                        re_to_return += scaledScore * exp(scaledScore) *
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
 * Use compositional score matrix adjustment, as described in
 *
 *     Altschul, Stephen F., John C. Wootton, E. Michael Gertz, Richa
 *     Agarwala, Aleksandr Morgulis, Alejandro A. Schaffer, and Yi-Kuo
 *     Yu (2005) "Protein database searches using compositionally
 *     adjusted substitution matrices", FEBS J.  272:5101-5109.
 *
 * to optimize a score matrix to a given set of letter frequencies.
 *
 * @param length1      adjusted length (not counting X) of the first
 *                     sequence
 * @param length2      adjusted length of the second sequence
 * @param probArray1   letter probabilities for the first sequence,
 *                     in the 20 letter amino-acid alphabet
 * @param probArray2   letter probabilities for the second sequence
 * @param pseudocounts number of pseudocounts to add the the
 *                     probabilities for each sequence, before optimizing
 *                     the scores.
 * @param specifiedRE  a relative entropy that might (subject to
 *                     fields in NRrecord) be used to as a constraint
 *                     of the optimization problem
 * @param NRrecord     a Blast_CompositionWorkspace that contains
 *                     fields used for the composition adjustment and
 *                     that will hold the output.
 * @param lambdaComputed   the new computed value of lambda
 *
 * @return 0 on success, 1 on failure to converge, -1 for out-of-memory
 */
int
Blast_CompositionMatrixAdj(int length1,
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
    default:  /* I assert that we can't get here */
        fprintf(stderr, "Unknown flag for setting relative entropy"
                "in composition matrix adjustment");
        exit(1);
    }
    status =
        Blast_OptimizeTargetFrequencies(&NRrecord->mat_final[0][0],
                                        COMPO_NUM_TRUE_AA,
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
                                  COMPO_NUM_TRUE_AA,
                                  NRrecord->mat_final,
                                  NRrecord->first_seq_freq_wpseudo,
                                  NRrecord->second_seq_freq_wpseudo);
        if (NRrecord->flag == eUnconstrainedRelEntropy) {
            /* Compute the unconstrained relative entropy */
            (void) Blast_CalcLambdaForComposition(NRrecord, 1, lambdaComputed);
        }
        /* success if and only if the computed lambda is positive */
        status = (*lambdaComputed > 0) ? 0 : 1;
    } else if (status == -1) {
        /* out of memory */
        status = -1;
    } else {
        /* Iteration did not converge */
        fprintf(stderr, "bad probabilities from sequence 1, length %d\n",
                length1);
        for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++)
            fprintf(stderr, "%15.12f\n", probArray1[i]);
        fprintf(stderr, "bad probabilities from sequence 2, length %d\n",
                length2);
        for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++)
            fprintf(stderr, "%15.12f\n", probArray2[i]);
        fflush(stderr);
        status = 1;
    }
    return status;
}


/**
 * Compute a compositionally adjusted scoring matrix.
 *
 * @param matrix        the adjusted matrix
 * @param query_composition       composition of the query sequence
 * @param queryLength             length of the query sequence
 * @param subject_composition     composition of the subject (database)
 *                                sequence
 * @param subjectLength           length of the subject sequence
 * @param matrixInfo    information about the underlying,
 *                      non-adjusted, scoring matrix.
 * @param RE_rule       the rule to use for computing the scoring
 *                      matrix
 * @param RE_pseudocounts    the number of pseudocounts to use in some
 *                           rules of composition adjustment
 * @param NRrecord      workspace used to perform compositional
 *                      adjustment
 * @param *whichMode    which mode of compositional adjustment was
 *                      actually used
 * @param calc_lambda   a function that can calculate the statistical
 *                      parameter Lambda from a set of score
 *                      frequencies.
 * @return              0 for success, 1 for failure to converge,
 *                      -1 for out of memory
 */
int
Blast_AdjustScores(Int4 ** matrix,
                   const Blast_AminoAcidComposition * query_composition,
                   int queryLength,
                   const Blast_AminoAcidComposition * subject_composition,
                   int subjectLength,
                   const Blast_MatrixInfo * matrixInfo,
                   int RE_rule,
                   int RE_pseudocounts,
                   Blast_CompositionWorkspace *NRrecord,
                   ECompoAdjustModes *whichMode,
                   double calc_lambda(double *,int,int,double))
{
    double LambdaRatio;      /* the ratio of the corrected
                                lambda to the original lambda */

    if (matrixInfo->positionBased || RE_rule == 0) {
        /* Use old-style composition-based statistics unconditionally. */
        *whichMode =  eCompoKeepOldMatrix;
        return Blast_CompositionBasedStats(matrix, &LambdaRatio,
                                           matrixInfo,
                                           query_composition->prob,
                                           subject_composition->prob,
                                           calc_lambda);
    } else {
        /* else call Yi-Kuo's code to choose mode for matrix adjustment. */

        /* The next two arrays are letter probabilities of query and
         * match in 20 letter ARND... alphabet. */
        double permutedQueryProbs[COMPO_NUM_TRUE_AA];
        double permutedMatchProbs[COMPO_NUM_TRUE_AA];

        s_GatherLetterProbs(permutedQueryProbs, query_composition->prob);
        s_GatherLetterProbs(permutedMatchProbs, subject_composition->prob);

        *whichMode =
            Blast_ChooseCompoAdjustMode(queryLength, subjectLength,
                                        permutedQueryProbs,
                                        permutedMatchProbs,
                                        matrixInfo->matrixName,
                                        RE_rule-1);
        /* compute and plug in new matrix here */
        if (eCompoKeepOldMatrix == *whichMode) {
            /* Yi-Kuo's code chose to use composition-based stats */
            return Blast_CompositionBasedStats(matrix, &LambdaRatio,
                                               matrixInfo,
                                               query_composition->prob,
                                               subject_composition->prob,
                                               calc_lambda);
        } else {
            /* else use compositionally adjusted scoring matrices */
            double correctUngappedLambda; /* new value of ungapped lambda */
            double ** REscoreMatrix = NULL;
            int status = 0;
            REscoreMatrix = Nlm_DenseMatrixNew(COMPO_PROTEIN_ALPHABET,
                                               COMPO_PROTEIN_ALPHABET);
            if (REscoreMatrix != NULL) {
                NRrecord->flag = *whichMode;
                status =
                    Blast_CompositionMatrixAdj(query_composition->
                                               numTrueAminoAcids,
                                               subject_composition->
                                               numTrueAminoAcids,
                                               permutedQueryProbs,
                                               permutedMatchProbs,
                                               RE_pseudocounts,
                                               kFixedReBlosum62,
                                               NRrecord,
                                               &correctUngappedLambda);
                if (status == 0) {
                    LambdaRatio =
                        correctUngappedLambda / matrixInfo->ungappedLambda;
                    if (LambdaRatio <= 0) {
                        status = 1;
                    }
                }
                if (status == 0) {
                    s_ScatterScores(REscoreMatrix, LambdaRatio,
                                    NRrecord->score_final);
                    /*scale matrix in floating point*/
                    Blast_SetNonstandardAaScores(REscoreMatrix,
                                                 matrixInfo->startMatrix);
                    s_RoundScoreMatrix(matrix, REscoreMatrix,
                                       COMPO_PROTEIN_ALPHABET);
                }
                Nlm_DenseMatrixFree(&REscoreMatrix);
            }
            return status;
        } /* end else use compositionally adjusted scoring matrices */
    } /* end else call Yi-Kuo's code to choose mode for matrix adjustment. */
}
