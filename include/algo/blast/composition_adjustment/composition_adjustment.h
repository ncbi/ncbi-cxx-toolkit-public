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
 * @author E. Michael Gertz, Alejandro Schaffer, Yi-Kuo Yu
 *
 * Definitions used in compositional score matrix adjustment
 */

#ifndef __COMPOSITION_ADJUSTMENT__
#define __COMPOSITION_ADJUSTMENT__

#include <algo/blast/core/blast_export.h>
#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/composition_adjustment/compo_mode_condition.h>

/* Number of standard amino acids */
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
struct Blast_AminoAcidComposition {
    double prob[26];         /**< probabilities of each amino acid, including
                                  nonstandard amino acids */
    int numTrueAminoAcids;   /**< number of true amino acids in the sequence,
                                  omitting X characters */
};
typedef struct Blast_AminoAcidComposition Blast_AminoAcidComposition;

NCBI_XBLAST_EXPORT
void
Blast_ReadAaComposition(Blast_AminoAcidComposition * composition,
                           const Uint1 * sequence, int length);

struct Blast_MatrixInfo {
    char * matrixName;         /**< name of the matrix */
    Int4    **startMatrix;     /**< Rescaled values of the original matrix */
    double **startFreqRatios;  /**< frequency ratios used to calculate matrix
                                    scores */
    int      rows;             /**< the number of rows in the scoring
                                    matrix. */
    int      positionBased;    /**< is the matrix position-based */
    double   ungappedLambda;   /**< ungapped Lambda value for this matrix
                                    in standard context */
};
typedef struct Blast_MatrixInfo Blast_MatrixInfo;

NCBI_XBLAST_EXPORT
Blast_MatrixInfo * Blast_MatrixInfoNew(int rows, int positionBased);

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

NCBI_XBLAST_EXPORT
Blast_CompositionWorkspace * Blast_CompositionWorkspaceNew();

NCBI_XBLAST_EXPORT
int Blast_CompositionWorkspaceInit(Blast_CompositionWorkspace * NRrecord,
                                   const char *matrixName);

NCBI_XBLAST_EXPORT
void Blast_CompositionWorkspaceFree(Blast_CompositionWorkspace ** NRrecord);

NCBI_XBLAST_EXPORT
void Blast_GetCompositionRange(int * pleft, int * pright,
                               const Uint1 * subject_data, int length,
                               int start, int finish);
NCBI_XBLAST_EXPORT
int
Blast_CompositionBasedStats(Int4 ** matrix, double * LambdaRatio,
                            const Blast_MatrixInfo * ss,
                            const double queryProb[], const double resProb[],
                            double (*calc_lambda)(double*,int,int,double));

NCBI_XBLAST_EXPORT
int Blast_CompositionMatrixAdj(int length1, int length2,
                               const double *probArray1,
                               const double *probArray2,
                               int pseudocounts, double specifiedRE,
                               Blast_CompositionWorkspace * NRrecord,
                               double * lambdaComputed);

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

NCBI_XBLAST_EXPORT
void Blast_Int4MatrixFromFreq(Int4 **matrix, int alphsize,
                              double ** freq, double Lambda);

NCBI_XBLAST_EXPORT
double Blast_GetRelativeEntropy(const double A[], const double B[]);

#ifdef __cplusplus
}
#endif

#endif
