#ifndef ALGO_BLAST_CORE___BLAST_PSI_PRIV__H
#define ALGO_BLAST_CORE___BLAST_PSI_PRIV__H

/*  $Id$
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
 * Author:  Alejandro Schaffer, ported by Christiam Camacho
 *
 */

/** @file blast_psi_priv.h
 * Private interface for Position Iterated BLAST API, contains the
 * PSSM generation engine.
 *
 * Calculating PSSMs from Seq-aligns is a multi-stage process. These stages
 * include:
 * 1) Processing the Seq-align
 *      Examine alignment and extract information about aligned characters,
 *      performed at the API level
 * 2) Purge biased sequences: construct M multiple sequence alignment as
 *      described in page 3395[1] - performed at the core level; custom
 *      selection of sequences should be performed at the API level.
 * 3) Compute extents of the alignment: M sub C as described in page 3395[1]
 * 4) Compute sequence weights
 * 5) Compute residue frequencies
 * 6) Convert residue frequencies to PSSM
 * 7) Scale the resulting PSSM
 */

#include <algo/blast/core/blast_psi.h>
#include "matrix_freq_ratios.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/* Extern declarations for constants (defined in blast_psi_priv.c) */

/** Index into PsiAlignmentData structure for the query sequence */
extern const unsigned int kQueryIndex;

/** Small constant to test against 0 */
extern const double kEpsilon;

/** FIXME: Should this value be replaced by BLAST_EXPECT_VALUE? */
extern const double kDefaultEvalueForPosition;

/** Successor to POSIT_SCALE_FACTOR  */
extern const int kPsiScaleFactor;


/****************************************************************************/
/* Matrix utility functions */

/** Generic 2 dimensional matrix allocator.
 * Allocates a ncols by nrows matrix with cells of size data_type_sz. Must be
 * freed using x_DeallocateMatrix
 * @param   ncols number of columns in matrix [in]
 * @param   nrows number of rows in matrix [in]
 * @param   data_type_sz size of the data type (in bytes) to allocate for each
 *          element in the matrix [in]
 * @return pointer to allocated memory or NULL in case of failure
 */
void**
_PSIAllocateMatrix(unsigned int ncols, unsigned int nrows, 
                   unsigned int data_type_sz);

/** Generic 2 dimensional matrix deallocator.
 * Deallocates the memory allocated by x_AllocateMatrix
 * @param matrix matrix to deallocate   [in]
 * @param ncols number of columns in the matrix [in]
 * @return NULL
 */
void**
_PSIDeallocateMatrix(void** matrix, unsigned int ncols);

/** Copies src matrix into dest matrix, both of which must be ncols by nrows 
 * matrices 
 * @param dest Destination matrix           [out]
 * @param src Source matrix                 [in]
 * @param ncols Number of columns to copy   [in]
 * @param ncows Number of rows to copy      [in]
 */
void
_PSICopyMatrix(double** dest, const double** src, 
               unsigned int ncols, unsigned int nrows);

/****************************************************************************/
/* Structure declarations */

/* FIXME: Should be renamed to extents? - this is what posExtents was in old 
   code, only using a simpler structure */

/** This structure keeps track of the regions aligned between the query
 * sequence and those that were not purged. It is used when calculating the
 * sequence weights */
typedef struct PsiAlignedBlock {
    SSeqRange* pos_extnt;     /**< Dynamically allocated array of size query_sz
                                  to keep track of the extents of each aligned
                                  position */

    Uint4* size;     /**< Dynamically allocated array of size query_sz
                                 that contains the size of the intervals in the
                                 array above */
} PsiAlignedBlock;

PsiAlignedBlock*
_PSIAlignedBlockNew(Uint4 num_positions);

PsiAlignedBlock*
_PSIAlignedBlockFree(PsiAlignedBlock* aligned_blocks);

/** FIXME: Where are the formulas for these? Need better names */
typedef struct PsiSequenceWeights {
    double** match_weights; /* observed residue frequencies (fi in paper) 
                               dimensions are query_sz+1 by PSI_ALPHABET_SIZE
                             */
    Uint4 match_weights_size;    /* kept for deallocation purposes */

    double* norm_seq_weights;   /**< Stores the normalized sequence weights
                                  (size num_seqs + 1) */
    double* row_sigma;  /**< array of num_seqs + 1 */
    /* Sigma: number of different characters occurring in matches within a
     * multi-alignment block - why is it a double? */
    double* sigma;      /**< array of num_seqs+1 (query_sz) length */

    double* std_prob;   /**< standard amino acid probabilities */

    /* These fields are required for important diagnostic output */
    double* gapless_column_weights; /**< FIXME */
    double* info_content;       /**< position information content (query_sz)*/

} PsiSequenceWeights;

PsiSequenceWeights*
_PSISequenceWeightsNew(const PsiInfo* info, const BlastScoreBlk* sbp);

PsiSequenceWeights*
_PSISequenceWeightsFree(PsiSequenceWeights* seq_weights);

/* Return values for internal PSI-BLAST functions */

#define PSI_SUCCESS             (0)
#define PSIERR_BADPARAM         (-1)   /**< Bad parameter used in function */
#define PSIERR_OUTOFMEM         (-2)   /**< Out of memory */
#define PSIERR_BADSEQWEIGHTS    (-3)   /**< Sequence weights do not add to 1 */
#define PSIERR_NOFREQRATIOS     (-4)   /**< No frequency ratios were found
                                         for the given scoring matrix */
#define PSIERR_POSITIVEAVGSCORE (-5)   /**< Positive average score found
                                         when scaling matrix */

/****************************************************************************/
/* Function prototypes for the various stages of the PSSM generation engine */

/** Main function for keeping only those selected sequences for PSSM
 * construction (stage 2)
 * FIXME: add boolean flag for custom selection of sequences?
 * @retval  PSIERR_BADPARAM if alignment is NULL
 *          PSI_SUCCESS otherwise
 */
int 
PSIPurgeBiasedSegments(PsiAlignmentData* alignment);

/** Main function to compute aligned blocks for each position within multiple 
 * alignment (stage 3) */
int
PSIComputeAlignmentBlocks(const PsiAlignmentData* alignment,    /* [in] */
                          PsiAlignedBlock* aligned_block);      /* [out] */

/** Main function to calculate the sequence weights. Should be called with the
 * return value of PSIComputeAlignmentBlocks (stage 4) */
int
PSIComputeSequenceWeights(const PsiAlignmentData* alignment,        /* [in] */
                          const PsiAlignedBlock* aligned_blocks,    /* [in] */
                          PsiSequenceWeights* seq_weights);         /* [out] */

/** Main function to compute the residue frequencies for the PSSM (stage 5) */
int
PSIComputeResidueFrequencies(const PsiAlignmentData* alignment,     /* [in] */
                             const PsiSequenceWeights* seq_weights, /* [in] */
                             const BlastScoreBlk* sbp,              /* [in] */
                             const PsiAlignedBlock* aligned_blocks, /* [in] */
                             const PSIBlastOptions* opts,           /* [in] */
                             PsiMatrix* score_matrix);              /* [out] */

/** Converts the residue frequencies obtained in the previous stage to a PSSM
 * (stage 6) */
int
PSIConvertResidueFreqsToPSSM(PsiMatrix* score_matrix,           /* [in|out] */
                             const Uint1* query,                /* [in] */
                             const BlastScoreBlk* sbp,          /* [in] */
                             const double* std_probs);          /* [in] */

/** Scales the PSSM (stage 7)
 * @param scaling_factor if not null, use this value to further scale the
 * matrix (default is kPsiScaleFactor). Useful for composition based statistics
 * [in] optional 
 */
int
PSIScaleMatrix(const Uint1* query,              /* [in] */
               Uint4 query_length,              /* [in] */
               const double* std_probs,         /* [in] */
               double* scaling_factor,          /* [in - optional] */
               PsiMatrix* score_matrix,         /* [in|out] */
               BlastScoreBlk* sbp);             /* [in|out] */

/****************************************************************************/
/* Function prototypes for auxiliary functions for the stages above */

/** Marks the (start, stop] region corresponding to sequence seq_index in
 * alignment so that it is not further considered for PSSM calculation.
 * This function is not applicable to the query sequence in the alignment
 * (seq_index == 0)
 * @param   alignment Alignment data  [in|out]
 * @param   seq_index index of the sequence of interested in alignment [in]
 * @param   start start of the region to remove [in]
 * @param   stop stop of the region to remove [in]
 * @return  PSIERR_BADPARAM if no alignment is given, or if seq_index or stop
 *          are invalid, 
 *          PSI_SUCCESS otherwise
 */
int
_PSIPurgeAlignedRegion(PsiAlignmentData* alignment,
                       unsigned int seq_index,
                       unsigned int start,
                       unsigned int stop);

/** Checks for any positions in sequence seq_index still considered for PSSM 
 * construction. If none is found, the entire sequence is marked as unused.
 * @param alignment Alignment data
 * @param seq_idnex index of the sequence of interest
 */
void
_PSIDiscardIfUnused(PsiAlignmentData* alignment, unsigned int seq_index);

/** The the standard residue frequencies for a scoring system specified in the
 * BlastScoreBlk structure. This is a wrapper for Blast_ResFreqStdComp() from
 * blast_stat.c with a more intention-revealing name :) .
 * used in kappa.c?
 * Caller is responsible for deallocating return value via sfree().
 * @param sbp Score block structure [in]
 * @retval NULL if there is not enough memory otherwise an array of lenght
 *         sbp->alphabet_size with the standard background probabilities for 
 *         the scoring system requested.
 */
double*
_PSIGetStandardProbabilities(const BlastScoreBlk* sbp);

/** Calculates the length of the sequence without including any 'X' residues.
 * used in kappa.c
 * @param seq sequence to examine [in]
 * @param length length of the sequence above [in]
 * @retval number of non-X residues in the sequence
 */
Uint4
_PSISequenceLengthWithoutX(const Uint1* seq, Uint4 length);

/* Compute the probabilities for each score in the PSSM.
 * This is only valid for protein sequences.
 * Should this go in blast_stat.[hc]?
 * used in kappa.c in notposfillSfp()
 */
Blast_ScoreFreq*
_PSIComputeScoreProbabilities(const int** pssm,             /* [in] */
                              const Uint1* query,           /* [in] */
                              Uint4 query_length,           /* [in] */
                              const double* std_probs,      /* [in] */
                              const BlastScoreBlk* sbp);    /* [in] */

/** Collects "diagnostic" information from the process of creating the PSSM */
PsiDiagnostics*
_PSISaveDiagnostics(const PsiAlignmentData* alignment,
                    const PsiAlignedBlock* aligned_block,
                    const PsiSequenceWeights* seq_weights);

#ifdef __cplusplus
}
#endif


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.4  2004/05/28 16:00:10  camacho
 * + first port of PSSM generation engine
 *
 * Revision 1.3  2004/05/06 14:01:40  camacho
 * + _PSICopyMatrix
 *
 * Revision 1.2  2004/04/07 21:43:47  camacho
 * Removed unneeded #include directive
 *
 * Revision 1.1  2004/04/07 19:11:17  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif /* !ALGO_BLAST_CORE__BLAST_PSI_PRIV__H */
