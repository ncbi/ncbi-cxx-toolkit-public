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
 * @file compo_mode_condition.h
 * Declarations of functions used to choose the mode for
 * composition-based statistics.
 *
 * @author Alejandro Schaffer, Yi-Kuo Yu
 */

#ifndef __COMPO_MODE_CONDITION__
#define __COMPO_MODE_CONDITION__

#include <algo/blast/core/blast_export.h>
#include <algo/blast/composition_adjustment/composition_constants.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Choose how the relative entropy should be constrained based on
 * properties of the two sequences to be aligned.
 *
 * @param length1     length of the first sequence
 * @param length2     length of the second sequence
 * @param probArray1  arrays of probabilities for the first sequence, in
 *                    a 20 letter amino-acid alphabet
 * @param probArray2  arrays of probabilities for the other sequence
 * @param matrixName  name of the scoring matrix
 * @param composition_adjust_mode   requested mode of composition adjustment
 */
NCBI_XBLAST_EXPORT
EMatrixAdjustRule
Blast_ChooseMatrixAdjustRule(int length1, int length2,
                             const double * probArray1,
                             const double * probArray2,
                             const char * matrixName,
                             ECompoAdjustModes composition_adjust_mode);

#ifdef __cplusplus
}
#endif

#endif
