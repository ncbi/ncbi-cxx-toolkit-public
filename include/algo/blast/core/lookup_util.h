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
 * ===========================================================================
 */

/** @file lookup_util.h
 *  Utility functions for lookup table generation.
 */

#include <algo/blast/core/blast_def.h>
#ifndef UTIL__H
#define UTIL__H

#ifdef __cplusplus
extern "C" {
#endif

/** Integer exponentiation using right to left binary algorithm.
 *  See knuth TAOCP vol. 2, section 4.6.3.
 *
 * @param x x
 * @param n n
 * @return x to the n-th power
 */

Int4 iexp(Int4 x, Int4 n);

/**
 * Integer base two logarithm.
 *
 * @param x x
 * @return lg(x)
 */

Int4 ilog2(Int4 x);

/** Make a bit mask with the low-order x bits set
 *
 * @param x x
 * @return the mask
 */

Int4 makemask(Int4 x);

/**
 * generates a de Bruijn sequence containing all substrings
 * of length n over an alphabet of size k.
 * if alphabet is not NULL, use it as a translation table
 * for the output.
 * expects that "output" has already been allocated and is at
 * least k^n in size.
 *
 * @param n the number of letters in each word
 * @param k the size of the alphabet
 * @param output the output sequence
 * @param alphabet optional translation alphabet
 */

void debruijn(Int4 n, Int4 k, Uint1* output, Uint1* alphabet);

/** Given a list of query locations, estimate the number of words
 * that would need to be added to a lookup table. The estimate is
 * currently intended for nucleotide locations, and ignores ambiguities
 * and the actual width of a lookup table word
 * @param location A linked list of locations to index [in]
 * @return The apprixomate number of lookup table entries
 */
Int4 EstimateNumTableEntries(BlastSeqLoc* location);

#ifdef __cplusplus
}
#endif

#endif /* UTIL__H */
