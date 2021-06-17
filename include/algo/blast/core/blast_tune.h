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
 * Author:  Jason Papadopoulos
 *
 */

/** @file blast_tune.h
 * Compute a blastn word size appropriate for finding,
 * with high probability, alignments with specified length and 
 * percent identity.
 */

#ifndef ALGO_BLAST_CORE___BLAST_TUNE__H
#define ALGO_BLAST_CORE___BLAST_TUNE__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_export.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Given a minimum amount of identity and the minimum desired length
 *  of nucleotide alignments, find the largest blastn word size that
 *  will find random instances of those alignments with high 
 *  probability. Note that when blast is actually run, it is obviously
 *  still possible to find alignments that are shorter and/or have less
 *  identity than what is specified here. The returned word size is
 *  just a choice that makes it unlikely that ungapped blast will 
 *  miss alignments that exceed *both* minimums. The algorithm used 
 *  is described in
 *
 *  <PRE>
 *  Valer Gotea, Vamsi Veeramachaneni, and Wojciech Makalowski
 *  "Mastering seeds for genomic size nucleotide BLAST searches"
 *  Nucleic Acids Research, 2003, Vol 31, No. 23, pp 6935-6941
 *  </PRE>
 *
 * @param min_percent_identity How much identity is expected in
 *              random alignments. Less identity means the probability of
 *              finding such alignments is decreased [in]
 * @param min_align_length The smallest alignment length desired.
 *              Longer length gives blastn more leeway to find seeds
 *              and increases the computed probability that alignments
 *              will be found [in]
 * @return The optimal word size, or zero if the optimization 
 *         process failed
 */
NCBI_XBLAST_EXPORT
Int4 BLAST_FindBestNucleotideWordSize(double min_percent_identity,
                                      Int4 min_align_length);

#ifdef __cplusplus
}
#endif

/* @} */

#endif  /* ALGO_BLAST_CORE___BLAST_TUNE__H */
