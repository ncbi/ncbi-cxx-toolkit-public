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
 * @file optimize_target_freq.h
 * Routines for finding an optimal set of target frequencies for the
 * purpose of generating a compositionally adjusted score matrix.
 *
 * @author E. Michael Gertz
 */

#ifndef __OPTIMIZE_TARGET_FREQ__
#define __OPTIMIZE_TARGET_FREQ__

#include <algo/blast/core/blast_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Find an optimal set of target frequencies for the purpose of
 * generating a compositionally adjusted score matrix.
 *
 * @param x           On exit, the optimal set of target frequencies,
 *                    interpreted as a two dimensional array in
 *                    row-major order.  x need not be initialized on
 *                    entry; any initial value will be ignored.
 * @param alphsize    the size of the alphabet for this optimization
 *                    problem.
 * @param *iterations the total number of iterations used in finding
 *                    the target frequencies
 * @param q           a set of target frequencies from a standard
 *                    matrix
 * @param row_sums    the required row sums for the target frequencies;
 *                    the composition of one of the sequences being compared.
 * @param col_sums    the required column sums for the target frequencies;
 *                    the composition of the other sequence being compared.
 * @param constrain_rel_entropy   if true, constrain the relative
 *                                entropy of the optimal target
 *                                frequencies to equal
 *                                relative_entropy
 * @param relative_entropy  if constrain_rel_entropy is true, then this
 *                          is the required relative entropy for the
 *                          optimal target frequencies.  Otherwise,
 *                          this argument is ignored.
 * @param maxits    the maximum number of iterations permitted for the
 *                  optimization algorithm; a good value is 2000.
 * @param tol       the solution tolerance; the residuals of the optimization
 *                  program must have Euclidean norm <= tol for the
 *                  algorithm to terminate.
 *
 * @returns         if an optimal set of target frequencies is
 *                  found, then 0, if the iteration failed to
 *                  converge, then 1, if there was some error, then -1.
 */
NCBI_XBLAST_EXPORT
int
Blast_OptimizeTargetFrequencies(double x[],
                                int alphsize,
                                int * iterations,
                                const double q[],
                                const double row_sums[],
                                const double col_sums[],
                                int constrain_rel_entropy,
                                double relative_entropy,
                                double tol,
                                int maxits);

#ifdef __cplusplus
}
#endif

#endif
