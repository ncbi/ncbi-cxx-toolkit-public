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
 * ==========================================================================*/
/**
 * @file composition_adjustment.h
 * Definitions used in compositional score matrix adjustment
 *
 * @author E. Michael Gertz, Alejandro Schaffer, Yi-Kuo Yu
 */

#ifndef __COMPOSITION_ADJUSTMENT__
#define __COMPOSITION_ADJUSTMENT__

#include <algo/blast/core/blast_export.h>
#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/composition_adjustment/compo_mode_condition.h>
#include <algo/blast/composition_adjustment/composition_constants.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Some characters in the NCBIstdaa alphabet, including ambiguity
   characters, selenocysteine and the stop character. */
enum { eGapChar = 0, eBchar = 2,  eDchar = 4,  eEchar = 5, eIchar = 9,
       eLchar = 11,  eNchar = 13, eQchar = 15, eXchar = 21,
       eZchar = 23,  eSelenocysteine = 24, eStopChar = 25,
       eOchar = 26,  eJchar = 27};

/**
 * Represents the composition of an amino-acid sequence, in the ncbistdaa
 * alphabet. */
typedef struct Blast_AminoAcidComposition {
    /** probabilities of each amino acid */
    double prob[COMPO_LARGEST_ALPHABET];
    int numTrueAminoAcids;   /**< number of true amino acids in the sequence,
                                  omitting nonstandard amino acids */
} Blast_AminoAcidComposition;


/**
 * Compute the true amino acid composition of a sequence, ignoring
 * ambiguity characters and other nonstandard characters.
 *
 * @param composition      the computed composition
 * @param alphsize         the size of the alphabet
 * @param sequence         a sequence of amino acids
 * @param length           length of the sequence
 */
NCBI_XBLAST_EXPORT
void Blast_ReadAaComposition(Blast_AminoAcidComposition * composition,
                             int alphsize,
                             const Uint1 * sequence, int length);


/** Information about a amino-acid substitution matrix */
typedef struct Blast_MatrixInfo {
    char * matrixName;         /**< name of the matrix */
    int    **startMatrix;     /**< Rescaled values of the original matrix */
    double **startFreqRatios;  /**< frequency ratios used to calculate matrix
                                    scores */
    int      rows;             /**< the number of rows in the scoring
                                    matrix. */
    int      cols;             /**< the number of columns in the scoring
                                    matrix, i.e. the alphabet size. */
    int      positionBased;    /**< is the matrix position-based */
    double   ungappedLambda;   /**< ungapped Lambda value for this matrix
                                    in standard context */
} Blast_MatrixInfo;


/** Create a Blast_MatrixInfo object
 *
 *  @param rows        the number of rows in the matrix, should be equal
 *                     to the size of the alphabet unless the matrix is
 *                     position based, in which case it is the query length
 *  @param cols        the number of columns in the matrix; the size of the
 *                     alphabet
 *  @param positionBased  is this matrix position-based?
 */
NCBI_XBLAST_EXPORT
Blast_MatrixInfo * Blast_MatrixInfoNew(int rows, int cols, int positionBased);


/** Free memory associated with a Blast_MatrixInfo object */
NCBI_XBLAST_EXPORT
void Blast_MatrixInfoFree(Blast_MatrixInfo ** ss);


/** Work arrays used to perform composition-based matrix adjustment */
typedef struct Blast_CompositionWorkspace {
    double ** mat_b;       /**< joint probabilities for the matrix in
                                standard context */
    double ** mat_final;   /**< optimized target frequencies */

    double * first_standard_freq;     /**< background frequency vector
                                           of the first sequence */
    double * second_standard_freq;    /**< background frequency vector of
                                           the second sequence */
} Blast_CompositionWorkspace;


/** Create a new Blast_CompositionWorkspace object, allocating memory
 * for all its component arrays. */
NCBI_XBLAST_EXPORT
Blast_CompositionWorkspace * Blast_CompositionWorkspaceNew(void);


/** Initialize the fields of a Blast_CompositionWorkspace for a specific
 * underlying scoring matrix. */
NCBI_XBLAST_EXPORT
int Blast_CompositionWorkspaceInit(Blast_CompositionWorkspace * NRrecord,
                                   const char *matrixName);


/** Free memory associated with a record of type
 * Blast_CompositionWorkspace. */
NCBI_XBLAST_EXPORT
void Blast_CompositionWorkspaceFree(Blast_CompositionWorkspace ** NRrecord);

/**
 * Compute the entropy of the scoring matrix implicit in a set of
 * target substitution frequencies.
 *
 * It is assumed that the background frequencies of the sequences
 * being compared are consistent with the substitution frequencies.
 */
NCBI_XBLAST_EXPORT
double Blast_TargetFreqEntropy(double ** target_freq);


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
NCBI_XBLAST_EXPORT
void Blast_GetCompositionRange(int * pleft, int * pright,
                               const Uint1 * subject_data, int length,
                               int start, int finish);


/**
 * Use composition-based statistics to adjust the scoring matrix, as
 * described in
 *
 *     Schaffer, A.A., Aravind, L., Madden, T.L., Shavirin, S.,
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
 * @param pValueAdjustment are unified p values being applied
 * @return 0 on success, -1 on out of memory
 */
NCBI_XBLAST_EXPORT
int
Blast_CompositionBasedStats(int ** matrix, double * LambdaRatio,
                            const Blast_MatrixInfo * ss,
                            const double queryProb[], const double resProb[],
                            double (*calc_lambda)(double*,int,int,double),
                            int pValueAdjustment);


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
 * @param matrix       the newly computed matrix [out]
 * @param alphsize     the size of the alphabet [in]
 * @param matrix_adjust_rule    the rule to use when computing the matrix;
 *                              affects how the relative entropy is
 *                              constrained
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
 * @param matrixInfo   information about the underlying, non-adjusted,
 *                     scoring matrix.
 *
 * @return 0 on success, 1 on failure to converge, -1 for out-of-memory
 */
NCBI_XBLAST_EXPORT
int Blast_CompositionMatrixAdj(int ** matrix,
                               int alphsize,
                               EMatrixAdjustRule matrix_adjust_rule,
                               int length1, int length2,
                               const double *probArray1,
                               const double *probArray2,
                               int pseudocounts, double specifiedRE,
                               Blast_CompositionWorkspace * NRrecord,
                               const Blast_MatrixInfo * matrixInfo);


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
 * @param composition_adjust_mode   mode of composition-based statistics
 *                                  to use
 * @param RE_pseudocounts    the number of pseudocounts to use in some
 *                           rules of composition adjustment
 * @param NRrecord      workspace used to perform compositional
 *                      adjustment
 * @param *matrix_adjust_rule    rule used to compute the scoring matrix
 *                      actually used
 * @param calc_lambda   a function that can calculate the statistical
 *                      parameter Lambda from a set of score
 *                      frequencies.
 * @param *pvalueForThisPair  used to get a composition p-value back
 * @param compositionTestIndex rule to decide on applying unified p-values
 * @param *ratioToPassBack lambda ratio to pass back for debugging
 * @return              0 for success, 1 for failure to converge,
 *                      -1 for out of memory
 */
NCBI_XBLAST_EXPORT
int
Blast_AdjustScores(int ** matrix,
                   const Blast_AminoAcidComposition * query_composition,
                   int queryLength,
                   const Blast_AminoAcidComposition * subject_composition,
                   int subjectLength,
                   const Blast_MatrixInfo * matrixInfo,
                   ECompoAdjustModes composition_adjust_mode,
                   int RE_pseudocounts,
                   Blast_CompositionWorkspace *NRrecord,
                   EMatrixAdjustRule *matrix_adjust_rule,
                   double calc_lambda(double *,int,int,double),
                   double *pvalueForThisPair,
                   int compositionTestIndex,
                   double *ratioToPassBack);


/**
 * Compute an integer-valued amino-acid score matrix from a set of
 * score frequencies.
 *
 * @param matrix       the preallocated matrix
 * @param size         size of the matrix
 * @param freq         a set of score frequencies
 * @param Lambda       the desired scale of the matrix
 */
NCBI_XBLAST_EXPORT
void Blast_Int4MatrixFromFreq(int **matrix, int size, 
                              double ** freq, double Lambda);


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
NCBI_XBLAST_EXPORT
double Blast_GetRelativeEntropy(const double A[], const double B[]);


/**
 * Compute the relative entropy of the scoring matrix that is
 * consistent with a set of target frequencies (old frequencies) when
 * that matrix is applied to a search with a different (new)
 * compositional context.
 *
 * @param entropy       the computed entropy
 * @param Lambda        the implicit scale of the matrix in the new context
 * @param iter_count    the number of iterations used in computing Lambda;
 *                      provided for display purposes
 * @param target_freq   20x20 matrix of target frequencies for the ARND...
 *                      alphabet
 * @param row_prob      residue probabilities for the sequence corresponding
 *                      to the rows of the matrix; not usually consistent
 *                      with target_freq
 * @param col_prob      residue probabilities for the sequence corresponding
 *                      to the columns of the matrix
 * @return    zero on success; -1 on out of memory; 1 on failure.
 *
 * A nonzero return is rare, and typically indicates that the entropy
 * could not be computed because the matrix had positive average
 * score, or no positive score with nonzero probability
 */
NCBI_XBLAST_EXPORT
int Blast_EntropyOldFreqNewContext(double * entropy, double * Lambda,
                                   int * iter_count, double ** target_freq,
                                   const double row_prob[],
                                   const double col_prob[]);

/**
 * Convert a matrix of target frequencies for the ARND alphabet of
 * true amino acids to a set of target frequencies for the NCBIstdaa
 * alphabet, filling in value for the two-character ambiguities (but
 * not X).
 *
 * @param StdFreq      frequencies in the NCBIstdaa alphabet [output]
 * @param StdAlphsize  the size of the NCBIstdaa alphabet [input]
 * @param freq         frequencies in the ARND alphabet [input]
 */
NCBI_XBLAST_EXPORT
void
Blast_TrueAaToStdTargetFreqs(double ** StdFreq, int StdAlphsize,
                             double ** freq);


/**
 * Convert a matrix of frequency ratios to a matrix of scores.
 * @param matrix            the matrix
 * @param rows              number of rows in the matrix
 * @param cols              number of rows in the matrix
 * @param Lambda            scale of the scores
 */
NCBI_XBLAST_EXPORT
void
Blast_FreqRatioToScore(double ** matrix, int rows, int cols, double Lambda);


/**
 *  Given a matrix of target frequencies, divide all elements by the
 *  character probabilities to get a matrix of frequency ratios.
 *
 *  @param ratios       on entry, target frequencies; on exit, frequency
 *                      ratios
 *  @param alphsize     size of the alphabet
 *  @param row_prob     character probabilities in the sequence corresponding
 *                      to the rows of "ratios"
 *  @param col_prob     character probabilities in the sequence corresponding
 *                      to the columns of "ratios"
 *
 * For any indices i, j for which row_prob[i] == 0 or col_prob[j] == 0, the
 * matrix entry is untouched; it is assumed that the calling routine knows
 * how to deal with these entries, since this routine does not.
 */
NCBI_XBLAST_EXPORT
void Blast_CalcFreqRatios(double ** ratios, int alphsize,
                          double row_prob[], double col_prob[]);


/**
 * Find the weighted average of a set of observed probabilities with a
 * set of "background" probabilities.  All array parameters have
 * length COMPO_NUM_TRUE_AA (i.e. 20).
 *
 * @param probs20                 on entry, observed probabilities; on
 *                                exit, weighted average probabilities.
 * @param number_of_observations  the number of characters used to
 *                                form the observed_freq array
 * @param background_probs20      the probability of characters in a
 *                                standard sequence.
 * @param pseudocounts            the number of "standard" characters
 *                                to be added to form the weighted
 *                                average.
 */
NCBI_XBLAST_EXPORT
void Blast_ApplyPseudocounts(double * probs20,
                             int number_of_observations,
                             const double * background_probs20,
                             int pseudocounts);

/**
 * Given a score matrix the character frequencies in two sequences,
 * compute the ungapped statistical parameter Lambda.
 *
 * If the average score for a composition is negative, then
 * statistical parameter Lambda exists and is the unique, positive
 * solution to
 *
 *    phi(lambda) = sum_{i,j} P_1(i) P_2(j) exp(S_{ij} lambda) - 1 = 0,
 *
 * where S_{ij} is the matrix "score" and P_1 and P_2 are row_probs and
 * col_probs respectively.
 *
 * @param *plambda      the computed lambda
 * @param *piterations  the number of iterations needed to compute Lambda,
 *                      or max_iterations if Lambda could not be computed.
 * @param score         a scoring matrix
 * @param alphsize      the size of the alphabet
 * @param row_prob      the frequencies for the sequence corresponding to
 *                      the rows of the matrix
 * @param col_prob      the frequencies for the sequence corresponding to
 *                      the columns of the matrix
 * @param lambda_tolerance     the desired relative precision for Lambda
 * @param function_tolerance   the desired maximum magnitude for
 *                             phi(lambda)
 * @param max_iterations the maximum number of permitted iterations.
 *
 * Note that Lambda does not exist unless the average score is negative and
 * the largest score that occurs with nonzero probability is positive.
 *
 * Comments on the algorithm used may be found in
 * composition_adjustment.c.
 */
NCBI_XBLAST_EXPORT
void Blast_CalcLambdaFullPrecision(double * plambda, int *piterations,
                                   double **score, int alphsize,
                                   const double row_prob[],
                                   const double col_prob[],
                                   double lambda_tolerance,
                                   double function_tolerance,
                                   int max_iterations);

/**
 * Calculate the entropy of a matrix relative to background
 * probabilities for two sequences.
 *
 * @param matrix       a scoring matrix
 * @param alphsize     size of the alphabet for matrix
 * @param row_prob     probabilities for the sequence corresponding to
 *                     the rows of matrix.
 * @param col_prob     probabilities for the sequence corresponding to
 *                     the columns of matrix.
 * @param Lambda       the statistical parameter Lambda for the
 *                     scoring system; can be calculated from the
 *                     matrix and the sequence probabilities, but it
 *                     is assumed to already be known.
 */
NCBI_XBLAST_EXPORT
double
Blast_MatrixEntropy(double ** matrix, int alphsize, const double row_prob[],
                    const double col_prob[], double Lambda);

#ifdef __cplusplus
}
#endif

#endif
