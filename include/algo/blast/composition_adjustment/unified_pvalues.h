/*  $Id$
 * ===========================================================================
 *
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
 * @file unified_pvalues.h
 * Headers for computing a "composition" p-value of a match, and for
 * computing a unified p-value combining the customary alignment
 * p-value and the new composition p-value
 *
 * @author Yi-Kuo Yu, Alejandro Schaffer, Mike Gertz
 */

#ifndef __UNIFIED_PVALUES__
#define __UNIFIED_PVALUES__

#include <algo/blast/core/blast_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** the smallest value of lambda in the table of lambda's empirical
 * distribution function. */  
#define COMPO_MIN_LAMBDA       0.034


/**
 * Conditionally compute a compositional p-value.
 *
 * @param lambda statistical parameter lambda estimated for this pair
 * @return the p-value
 */
NCBI_XBLAST_EXPORT
double Blast_CompositionPvalue(double lambda);

/**
 * This function implements the method of Fisher, R. C. Elston (1991)
 * Biometrical J. 33:339-345 and T. L. Bailey and M. Gribskov (1998)
 * Bioinformatics 14:48-54.  to combine to p-values into a unified
 * p-value.  The input p-values are p_comp and p_alignment. The value
 * returned, call it p_return, is the area in the unit square under
 * the curve y = p_comp*p_align.
 *
 * @param p_comp  composition p-value
 * @param p_alignment alignment p-value
 */
NCBI_XBLAST_EXPORT
double Blast_Overall_P_Value(double p_comp,
                             double p_alignment);

#ifdef __cplusplus
}
#endif

#endif
