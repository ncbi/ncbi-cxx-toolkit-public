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
 * ===========================================================================*/
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

/** Number of standard amino acids */
#define COMPO_NUM_TRUE_AA 20

#ifdef __cplusplus
extern "C" {
#endif

/* Some characters in the 26 letter NCBIstdaa alphabet, including
   ambiguity characters, selenocysteine and the stop character. */
enum { eGapChar = 0, eBchar = 2, eDchar = 4, eEchar = 5, eNchar = 13,
       eQchar = 15, eXchar = 21, eZchar = 23, eSelenocysteine = 24,
       eStopChar = 25};

/**
 * Represents the composition of an amino-acid sequence */
typedef struct Blast_AminoAcidComposition {
    double prob[26];         /**< probabilities of each amino acid, including
                                  nonstandard amino acids */
    int numTrueAminoAcids;   /**< number of true amino acids in the sequence,
                                  omitting X characters */
} Blast_AminoAcidComposition;


/**
 * Compute the amino acid composition of a sequence.
 *
 * @param composition      the computed composition
 * @param sequence         a sequence of amino acids
 * @param length           length of the sequence
 */
NCBI_XBLAST_EXPORT
void
Blast_ReadAaComposition(Blast_AminoAcidComposition * composition,
                           const Uint1 * sequence, int length);


/** Information about a amino-acid substitution matrix */
typedef struct Blast_MatrixInfo {
    char * matrixName;         /**< name of the matrix */
    Int4    **startMatrix;     /**< Rescaled values of the original matrix */
    double **startFreqRatios;  /**< frequency ratios used to calculate matrix
                                    scores */
    int      rows;             /**< the number of rows in the scoring
                                    matrix. */
    int      positionBased;    /**< is the matrix position-based */
    double   ungappedLambda;   /**< ungapped Lambda value for this matrix
                                    in standard context */
} Blast_MatrixInfo;


/** Create a Blast_MatrixInfo object
 *
 *  @param rows        the number of rows in the matrix, should be
 *                     COMPO_PROTEIN_ALPHABET unless the matrix is position
 *                     based, in which case it is the query length
 *  @param positionBased  is this matrix position-based?
 */
NCBI_XBLAST_EXPORT
Blast_MatrixInfo * Blast_MatrixInfoNew(int rows, int positionBased);


/** Free memory associated with a Blast_MatrixInfo object */
NCBI_XBLAST_EXPORT
void Blast_MatrixInfoFree(Blast_MatrixInfo ** ss);


/** Work arrays used to perform composition-based matrix adjustment */
typedef struct Blast_CompositionWorkspace {
    int flag;              /**< determines which of the optimization
                                problems are solved */
    double ** mat_b;       /**< joint probabilities for the matrix in
                                standard context */
    double ** score_old;   /**< score of the matrix in standard context
                                with scale Lambda == 1 */
    double ** mat_final;   /**< optimized target frequencies */
    double ** score_final; /**< optimized score matrix */

    double RE_final;       /**< the relative entropy used, either
                                re_o_implicit or re_o_newcontext */
    double RE_o_implicit;  /**< used for eRelEntropyOldMatrixOldContext
                                mode */

    double * first_seq_freq;          /**< freq vector of first seq */
    double * second_seq_freq;         /**< freq. vector for the second. */
    double * first_standard_freq;     /**< background freq vector of first
                                           seq using matrix */
    double * second_standard_freq;    /**< background freq vector for
                                                   the second. */
    double * first_seq_freq_wpseudo;  /**< freq vector of first seq
                                           w/pseudocounts */
    double * second_seq_freq_wpseudo; /**< freq. vector for the
                                           second seq w/pseudocounts */
} Blast_CompositionWorkspace;


/** Create a new Blast_CompositionWorkspace object, allocating memory
 * for all its component arrays. */
NCBI_XBLAST_EXPORT
Blast_CompositionWorkspace * Blast_CompositionWorkspaceNew();


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
NCBI_XBLAST_EXPORT
int
Blast_CompositionBasedStats(int ** matrix, double * LambdaRatio,
                            const Blast_MatrixInfo * ss,
                            const double queryProb[], const double resProb[],
                            double (*calc_lambda)(double*,int,int,double));


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
NCBI_XBLAST_EXPORT
int Blast_CompositionMatrixAdj(int length1, int length2,
                               const double *probArray1,
                               const double *probArray2,
                               int pseudocounts, double specifiedRE,
                               Blast_CompositionWorkspace * NRrecord,
                               double * lambdaComputed);


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
NCBI_XBLAST_EXPORT
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
                   double calc_lambda(double *,int,int,double));


/**
 * Compute an integer-valued amino-acid score matrix from a set of
 * score frequencies.
 *
 * @param matrix       the preallocated matrix
 * @param alphsize     the size of the alphabet for this matrix
 * @param freq         a set of score frequencies
 * @param Lambda       the desired scale of the matrix
 */
NCBI_XBLAST_EXPORT
void Blast_Int4MatrixFromFreq(Int4 **matrix, int alphsize,
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

#ifdef __cplusplus
}
#endif

#endif
