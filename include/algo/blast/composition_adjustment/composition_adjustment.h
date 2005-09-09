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

File name: compostion_adjustment.h

Authors: E. Michael Gertz, Alejandro Schaffer, Yi-Kuo Yu

Contents: Definitions used in compositional score matrix adjustment
******************************************************************************/

#ifndef __COMPOSITION_ADJUSTMENT__
#define __COMPOSITION_ADJUSTMENT__

#define COMPOSITION_ALPHABET_SIZE 20

#ifdef __cplusplus
extern "C" {
#endif

/* Next five constants are the options for how relative entropy is
   specified */
enum ECompoAdjustModes { 
    eNoCompositionAdjustment       = (-1),
    eCompoKeepOldMatrix            = 0,
    eUnconstrainedRelEntropy       = 1,
    eRelEntropyOldMatrixNewContext = 2,
    eRelEntropyOldMatrixOldContext = 3,
    eUserSpecifiedRelEntropy       = 4,
    eNumCompoAdjustModes
};
typedef enum ECompoAdjustModes ECompoAdjustModes;

int
Blast_GetJointProbsForMatrix(double ** probs, double row_sums[],
                             double col_sums[], const char *matrixName);
void
Blast_ApplyPseudocounts(double * probs_with_pseudo, 
                        int length,
                        double * normalized_probs,
                        const double * observed_freq,
                        const double * background_probs,
                        int pseudocounts);

double Blast_GetRelativeEntropy(const double A[], const double B[]);
void
Blast_ScoreMatrixFromFreq(double ** score, int alphsize, double ** freq,
                          const double row_sum[], const double col_sum[]);


#ifdef __cplusplus
}
#endif

#endif
