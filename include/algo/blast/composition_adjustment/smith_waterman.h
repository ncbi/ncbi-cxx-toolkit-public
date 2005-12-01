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
 * @author Alejandro Schaffer, E. Michael Gertz
 *
 * Definitions for computing Smith-Waterman alignments
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

NCBI_XBLAST_EXPORT
int Blast_ForbiddenRangesInitialize(Blast_ForbiddenRanges * self,
                                    int capacity);

NCBI_XBLAST_EXPORT
void Blast_ForbiddenRangesClear(Blast_ForbiddenRanges * self);

NCBI_XBLAST_EXPORT
int Blast_ForbiddenRangesPush(Blast_ForbiddenRanges * self,
                              int queryStart, int queryEnd,
                              int matchStart, int matchEnd);

NCBI_XBLAST_EXPORT
void Blast_ForbiddenRangesRelease(Blast_ForbiddenRanges * self);

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
