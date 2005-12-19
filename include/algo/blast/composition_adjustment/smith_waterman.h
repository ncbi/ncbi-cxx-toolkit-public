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
 * @file smith_waterman.h
 * Definitions for computing Smith-Waterman alignments
 *
 * @author Alejandro Schaffer, E. Michael Gertz
 */
#ifndef __SMITH_WATERMAN__
#define __SMITH_WATERMAN__

#include <algo/blast/core/blast_export.h>
#include <algo/blast/core/ncbi_std.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * An instance of Blast_ForbiddenRanges is used by the Smith-Waterman
 * algorithm to represent ranges in the database that are not to be
 * aligned.
 */
typedef struct Blast_ForbiddenRanges {
    int   isEmpty;             /**< True if there are no forbidden ranges */
    int  *numForbidden;        /**< how many forbidden ranges at each
                                    database position */
    int **ranges;              /**< forbidden ranges for each database
                                   position */
    int   capacity;         /**< length of the query sequence */
} Blast_ForbiddenRanges;


/**
 * Initialize a new, empty Blast_ForbiddenRanges
 *
 * @param self              object to be initialized
 * @param capacity          the number of ranges that may be stored
 *                          (must be at least as long as the length
 *                           of the query)
 */
NCBI_XBLAST_EXPORT
int Blast_ForbiddenRangesInitialize(Blast_ForbiddenRanges * self,
                                    int capacity);


/** Reset self to be empty */
NCBI_XBLAST_EXPORT
void Blast_ForbiddenRangesClear(Blast_ForbiddenRanges * self);


/** Add some ranges to self
 * @param self          an instance of Blast_ForbiddenRanges [in][out]
 * @param queryStart    start of the alignment in the query sequence
 * @param queryEnd      the end of the alignment in the query sequence
 * @param matchStart    start of the alignment in the subject sequence
 * @param matchEnd      the end of the alignment in the subject sequence
 */
NCBI_XBLAST_EXPORT
int Blast_ForbiddenRangesPush(Blast_ForbiddenRanges * self,
                              int queryStart, int queryEnd,
                              int matchStart, int matchEnd);


/**
 * Release the storage associated with the fields of self, but do not
 * delete self
 *
 * @param self          an instance of Blast_ForbiddenRanges [in][out]
 */
NCBI_XBLAST_EXPORT
void Blast_ForbiddenRangesRelease(Blast_ForbiddenRanges * self);


/**
 * Find the left-hand endpoints of the locally optimal Smith-Waterman
 * alignment given the score and right-hand endpoints computed by
 * Blast_SmithWatermanScoreOnly.
 *
 * @param *score_out        the score of the optimal alignment -- should
 *                          equal score_in.
 * @param *matchSeqStart    the left-hand endpoint of the alignment in
 *                          the database sequence
 * @param *queryStart       the right-hand endpoint of the alignment
 *                          in the query sequence
 * @param subject_data      the database sequence data
 * @param subject_length    length of matchSeq
 * @param query_data        the query sequence data
 * @param matrix            amino-acid scoring matrix
 * @param gapOpen           penalty for opening a gap
 * @param gapExtend         penalty for extending a gap by one amino acid
 * @param matchSeqEnd       right-hand endpoint of the alignment in
 *                          the database sequence
 * @param queryEnd          right-hand endpoint of the alignment in
 *                          the query
 * @param score_in          the score of the alignment
 * @param positionSpecific  determines whether matrix is position
 *                          specific or not
 * @param forbiddenRanges   ranges that must not be included in the alignment
 *
 * @return 0 on success, -1 on out-of-memory
 */
NCBI_XBLAST_EXPORT
int Blast_SmithWatermanFindStart(int * score_out,
                                 int *matchSeqStart,
                                 int *queryStart,
                                 const Uint1 * subject_data,
                                 int subject_length,
                                 const Uint1 * query_data,
                                 int **matrix,
                                 int gapOpen,
                                 int gapExtend,
                                 int matchSeqEnd,
                                 int queryEnd,
                                 int score_in,
                                 int positionSpecific,
                                 const Blast_ForbiddenRanges *
                                 forbiddenRanges);
    
/**
 * Compute the score and right-hand endpoints of the locally optimal
 * Smith-Waterman alignment, possibly subject to the restriction that some
 * ranges are forbidden.
 *
 * @param *score            the computed score
 * @param *matchSeqEnd      the right-hand end of the alignment in the
 *                          database sequence
 * @param *queryEnd         the right-hand end of the alignment in the
 *                          query sequence
 * @param subject_data      the database sequence data
 * @param subject_length    length of matchSeq
 * @param query_data        the query sequence data
 * @param query_length      length of query
 * @param matrix            amino-acid scoring matrix
 * @param gapOpen           penalty for opening a gap
 * @param gapExtend         penalty for extending a gap by one amino acid
 * @param forbiddenRanges   lists areas that should not be aligned [in]
 * @param positionSpecific  determines whether matrix is position
 *                          specific or not
 * @return 0 on success; -1 on out-of-memory
 */
NCBI_XBLAST_EXPORT
int Blast_SmithWatermanScoreOnly(int *score,
                                 int *matchSeqEnd, int *queryEnd,
                                 const Uint1 * subject_data,
                                 int subject_length,
                                 const Uint1 * query_data,
                                 int query_length, int **matrix,
                                 int gapOpen, int gapExtend,
                                 int positionSpecific,
                                 const Blast_ForbiddenRanges *
                                 forbiddenRanges);

#ifdef __cplusplus
}
#endif

#endif
