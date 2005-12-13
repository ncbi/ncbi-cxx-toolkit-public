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

/**
 * @file smith_waterman.c
 *
 * Routines for computing rigorous, Smith-Waterman alignments.
 */
#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/composition_adjustment/composition_constants.h>
#include <algo/blast/composition_adjustment/smith_waterman.h>

/** A structure used internally by the Smith-Waterman algorithm to
 * represent gaps */
typedef struct SwGapInfo {
    int noGap;
    int gapExists;
} SwGapInfo;


/**
 * Compute the score and right-hand endpoints of the locally optimal
 * Smith-Waterman alignment.
 *
 * @param *score            the computed score
 * @param *matchSeqEnd      the right-hand end of the alignment in the
 *                          database sequence
 * @param *queryEnd         the right-hand end of the alignment in the
 *                          query sequence
 * @param matchSeq          the database sequence data
 * @param matchSeqLength    length of matchSeq
 * @param query             the query sequence data
 * @param queryLength       length of query
 * @param matrix            amino-acid scoring matrix
 * @param gapOpen           penalty for opening a gap
 * @param gapExtend         penalty for extending a gap by one amino acid
 * @param positionSpecific  determines whether matrix is position
 *                          specific or not
 */
static int
BLbasicSmithWatermanScoreOnly(int *score, int *matchSeqEnd, int *queryEnd,
                              const Uint1 * matchSeq, int matchSeqLength,
                              const Uint1 * query,    int queryLength,
                              int **matrix, int gapOpen, int gapExtend,
                              int positionSpecific)
{
    int bestScore;               /* best score seen so far */
    int newScore;                /* score of next entry */
    int bestMatchSeqPos, bestQueryPos; /* position ending best score in
                                          matchSeq and query sequences */
    SwGapInfo *scoreVector;      /* keeps one row of the
                                    Smith-Waterman matrix overwrite
                                    old row with new row */
    int *matrixRow;              /* one row of score matrix */
    int newGapCost;              /* cost to have a gap of one character */
    int prevScoreNoGapMatchSeq;  /* score one row and column up with
                                    no gaps */
    int prevScoreGapMatchSeq;    /* score if a gap already started in
                                    matchSeq */
    int continueGapScore;        /* score for continuing a gap in matchSeq */
    int matchSeqPos, queryPos;   /* positions in matchSeq and query */

    scoreVector = (SwGapInfo *) malloc(matchSeqLength * sizeof(SwGapInfo));
    if (scoreVector == NULL) {
        return -1;
    }
    bestMatchSeqPos = 0;
    bestQueryPos = 0;
    bestScore = 0;
    newGapCost = gapOpen + gapExtend;
    for (matchSeqPos = 0;  matchSeqPos < matchSeqLength;  matchSeqPos++) {
        scoreVector[matchSeqPos].noGap = 0;
        scoreVector[matchSeqPos].gapExists = -gapOpen;
    }
    for (queryPos = 0;  queryPos < queryLength;  queryPos++) {
        if (positionSpecific)
            matrixRow = matrix[queryPos];
        else
            matrixRow = matrix[query[queryPos]];
        newScore = 0;
        prevScoreNoGapMatchSeq = 0;
        prevScoreGapMatchSeq = -(gapOpen);
        for (matchSeqPos = 0;  matchSeqPos < matchSeqLength;  matchSeqPos++) {
            /* testing scores with a gap in matchSeq, either starting a
             * new gap or extending an existing gap*/
            if ((newScore = newScore - newGapCost) >
                (prevScoreGapMatchSeq = prevScoreGapMatchSeq - gapExtend))
                prevScoreGapMatchSeq = newScore;
            /* testing scores with a gap in query, either starting a
             * new gap or extending an existing gap*/
            if ((newScore = scoreVector[matchSeqPos].noGap - newGapCost) >
                (continueGapScore =
                 scoreVector[matchSeqPos].gapExists - gapExtend))
                continueGapScore = newScore;
            /* compute new score extending one position in matchSeq
             * and query */
            newScore =
                prevScoreNoGapMatchSeq + matrixRow[matchSeq[matchSeqPos]];
            if (newScore < 0)
                newScore = 0; /*Smith-Waterman locality condition*/
            /*test two alternatives*/
            if (newScore < prevScoreGapMatchSeq)
                newScore = prevScoreGapMatchSeq;
            if (newScore < continueGapScore)
                newScore = continueGapScore;
            prevScoreNoGapMatchSeq = scoreVector[matchSeqPos].noGap;
            scoreVector[matchSeqPos].noGap = newScore;
            scoreVector[matchSeqPos].gapExists = continueGapScore;
            if (newScore > bestScore) {
                bestScore = newScore;
                bestQueryPos = queryPos;
                bestMatchSeqPos = matchSeqPos;
            }
        }
    }
    free(scoreVector);
    if (bestScore < 0)
        bestScore = 0;
    *matchSeqEnd = bestMatchSeqPos;
    *queryEnd = bestQueryPos;
    *score = bestScore;

    return 0;
}


/**
 * Find the left-hand endpoints of the locally optimal Smith-Waterman
 * alignment given the score and right-hand endpoints computed by
 * BLbasicSmithWatermanScoreOnly.
 *
 * @param *score_out        the score of the optimal alignment -- should
 *                          equal score_in.
 * @param *matchSeqStart    the left-hand endpoint of the alignment in
 *                          the database sequence
 * @param *queryStart       the right-hand endpoint of the alignment
 *                          in the query sequence
 * @param matchSeq          the database sequence data
 * @param matchSeqLength    length of matchSeq
 * @param query             the query sequence data
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
 */
static int
BLSmithWatermanFindStart(int *score_out,
                         int *matchSeqStart, int *queryStart,
                         const Uint1 * matchSeq, int matchSeqLength,
                         const Uint1 *query,
                         int **matrix, int gapOpen, int gapExtend,
                         int matchSeqEnd, int queryEnd, int score_in,
                         int positionSpecific)
{
    int bestScore;               /* best score seen so far*/
    int newScore;                /* score of next entry*/
    int bestMatchSeqPos, bestQueryPos; /*position starting best score in
                                          matchSeq and database sequences */
    SwGapInfo *scoreVector;      /* keeps one row of the Smith-Waterman
                                    matrix overwrite old row with new row */
    int *matrixRow;              /* one row of score matrix */
    int newGapCost;              /* cost to have a gap of one character */
    int prevScoreNoGapMatchSeq;  /* score one row and column up
                                    with no gaps*/
    int prevScoreGapMatchSeq;    /* score if a gap already started in
                                    matchSeq */
    int continueGapScore;        /* score for continuing a gap in query */
    int matchSeqPos, queryPos;   /* positions in matchSeq and query */

    scoreVector = (SwGapInfo *) malloc(matchSeqLength * sizeof(SwGapInfo));
    if (scoreVector == NULL) {
        return -1;
    }
    bestMatchSeqPos = 0;
    bestQueryPos = 0;
    bestScore = 0;
    newGapCost = gapOpen + gapExtend;
    for (matchSeqPos = 0;  matchSeqPos < matchSeqLength;  matchSeqPos++) {
        scoreVector[matchSeqPos].noGap = 0;
        scoreVector[matchSeqPos].gapExists = -(gapOpen);
    }
    for (queryPos = queryEnd;  queryPos >= 0;  queryPos--) {
        if (positionSpecific)
            matrixRow = matrix[queryPos];
        else
            matrixRow = matrix[query[queryPos]];
        newScore = 0;
        prevScoreNoGapMatchSeq = 0;
        prevScoreGapMatchSeq = -(gapOpen);
        for (matchSeqPos = matchSeqEnd;  matchSeqPos >= 0;  matchSeqPos--) {
            /* testing scores with a gap in matchSeq, either starting
             * a new gap or extending an existing gap */
            if ((newScore = newScore - newGapCost) >
                (prevScoreGapMatchSeq = prevScoreGapMatchSeq - gapExtend))
                prevScoreGapMatchSeq = newScore;
            /* testing scores with a gap in query, either starting a
             * new gap or extending an existing gap */
            if ((newScore = scoreVector[matchSeqPos].noGap - newGapCost) >
                (continueGapScore =
                 scoreVector[matchSeqPos].gapExists - gapExtend))
                continueGapScore = newScore;
            /* compute new score extending one position in matchSeq
             * and query */
            newScore =
                prevScoreNoGapMatchSeq + matrixRow[matchSeq[matchSeqPos]];
            if (newScore < 0)
                newScore = 0; /* Smith-Waterman locality condition */
            /* test two alternatives */
            if (newScore < prevScoreGapMatchSeq)
                newScore = prevScoreGapMatchSeq;
            if (newScore < continueGapScore)
                newScore = continueGapScore;
            prevScoreNoGapMatchSeq = scoreVector[matchSeqPos].noGap;
            scoreVector[matchSeqPos].noGap = newScore;
            scoreVector[matchSeqPos].gapExists = continueGapScore;
            if (newScore > bestScore) {
                bestScore = newScore;
                bestQueryPos = queryPos;
                bestMatchSeqPos = matchSeqPos;
            }
            if (bestScore >= score_in)
                break;
        }
        if (bestScore >= score_in)
            break;
    }
    free(scoreVector);
    if (bestScore < 0)
        bestScore = 0;
    *matchSeqStart = bestMatchSeqPos;
    *queryStart = bestQueryPos;
    *score_out = bestScore;

    return 0;
}


/**
 * Compute the score and right-hand endpoints of the locally optimal
 * Smith-Waterman alignment, subject to the restriction that some
 * ranges are forbidden.
 *
 * @param *score            the computed score
 * @param *matchSeqEnd      the right-hand end of the alignment in the
 *                          database sequence
 * @param *queryEnd         the right-hand end of the alignment in the
 *                          query sequence
 * @param matchSeq          the database sequence data
 * @param matchSeqLength    length of matchSeq
 * @param query             the query sequence data
 * @param queryLength       length of query
 * @param matrix            amino-acid scoring matrix
 * @param gapOpen           penalty for opening a gap
 * @param gapExtend         penalty for extending a gap by one amino acid
 * @param numForbidden      number of forbidden ranges [in]
 * @param forbiddenRanges   lists areas that should not be aligned [in]
 * @param positionSpecific  determines whether matrix is position
 *                          specific or not
 */
static int
BLspecialSmithWatermanScoreOnly(int *score, int *matchSeqEnd, int *queryEnd,
                                const Uint1 * matchSeq, int matchSeqLength,
                                const Uint1 *query, int queryLength,
                                int **matrix, int gapOpen, int gapExtend,
                                const int *numForbidden,
                                int ** forbiddenRanges,
                                int positionSpecific)
{
    int bestScore;               /* best score seen so far */
    int newScore;                /* score of next entry*/
    int bestMatchSeqPos, bestQueryPos; /*position ending best score in
                                          matchSeq and database sequences */
    SwGapInfo *scoreVector;      /* keeps one row of the Smith-Waterman
                                    matrix overwrite old row with new row */
    int *matrixRow;              /* one row of score matrix */
    int newGapCost;              /* cost to have a gap of one character */
    int prevScoreNoGapMatchSeq;  /* score one row and column up
                                    with no gaps*/
    int prevScoreGapMatchSeq;    /* score if a gap already started in
                                    matchSeq */
    int continueGapScore;        /* score for continuing a gap in query */
    int matchSeqPos, queryPos;   /* positions in matchSeq and query */
    int forbidden;               /* is this position forbidden? */
    int f;                       /* index over forbidden positions */

    scoreVector = (SwGapInfo *) malloc(matchSeqLength * sizeof(SwGapInfo));
    if (scoreVector == NULL) {
        return -1;
    }
    bestMatchSeqPos = 0;
    bestQueryPos = 0;
    bestScore = 0;
    newGapCost = gapOpen + gapExtend;
    for (matchSeqPos = 0;  matchSeqPos < matchSeqLength;  matchSeqPos++) {
        scoreVector[matchSeqPos].noGap = 0;
        scoreVector[matchSeqPos].gapExists = -(gapOpen);
    }
    for (queryPos = 0;  queryPos < queryLength;  queryPos++) {
        if (positionSpecific)
            matrixRow = matrix[queryPos];
        else
            matrixRow = matrix[query[queryPos]];
        newScore = 0;
        prevScoreNoGapMatchSeq = 0;
        prevScoreGapMatchSeq = -(gapOpen);
        for (matchSeqPos = 0;  matchSeqPos < matchSeqLength;  matchSeqPos++) {
            /* testing scores with a gap in matchSeq, either starting
             * a new gap or extending an existing gap */
            if ((newScore = newScore - newGapCost) >
                (prevScoreGapMatchSeq = prevScoreGapMatchSeq - gapExtend))
                prevScoreGapMatchSeq = newScore;
            /* testing scores with a gap in query, either starting a
             * new gap or extending an existing gap */
            if ((newScore = scoreVector[matchSeqPos].noGap - newGapCost) >
                (continueGapScore =
                 scoreVector[matchSeqPos].gapExists - gapExtend))
                continueGapScore = newScore;
            /* compute new score extending one position in matchSeq
             * and query */
            forbidden = FALSE;
            for (f = 0;  f < numForbidden[queryPos];  f++) {
                if ((matchSeqPos >= forbiddenRanges[queryPos][2 * f]) &&
                    (matchSeqPos <= forbiddenRanges[queryPos][2*f + 1])) {
                    forbidden = TRUE;
                    break;
                }
            }
            if (forbidden)
                newScore = COMPO_SCORE_MIN;
            else
                newScore =
                    prevScoreNoGapMatchSeq + matrixRow[matchSeq[matchSeqPos]];
            if (newScore < 0)
                newScore = 0; /* Smith-Waterman locality condition */
            /* test two alternatives */
            if (newScore < prevScoreGapMatchSeq)
                newScore = prevScoreGapMatchSeq;
            if (newScore < continueGapScore)
                newScore = continueGapScore;
            prevScoreNoGapMatchSeq = scoreVector[matchSeqPos].noGap;
            scoreVector[matchSeqPos].noGap = newScore;
            scoreVector[matchSeqPos].gapExists = continueGapScore;
            if (newScore > bestScore) {
                bestScore = newScore;
                bestQueryPos = queryPos;
                bestMatchSeqPos = matchSeqPos;
            }
        }
    }
    free(scoreVector);
    if (bestScore < 0)
        bestScore = 0;
    *matchSeqEnd = bestMatchSeqPos;
    *queryEnd = bestQueryPos;
    *score = bestScore;

    return 0;
}


/**
 * Find the left-hand endpoints of the locally optimal Smith-Waterman
 * alignment, subject to the restriction that certain ranges may not
 * be aligned, given the score and right-hand endpoints computed by
 * BLspecialSmithWatermanScoreOnly.
 *
 * @param *score_out        the score of the optimal alignment -- should
 *                          equal score_in.
 * @param *matchSeqStart    the left-hand endpoint of the alignment in
 *                          the database sequence
 * @param *queryStart       the right-hand endpoint of the alignment
 *                          in the query sequence
 * @param matchSeq          the database sequence data
 * @param matchSeqLength    length of matchSeq
 * @param query             the query sequence data
 * @param matrix            amino-acid scoring matrix
 * @param gapOpen           penalty for opening a gap
 * @param gapExtend         penalty for extending a gap by one amino acid
 * @param matchSeqEnd       right-hand endpoint of the alignment in
 *                          the database sequence
 * @param queryEnd          right-hand endpoint of the alignment in
 *                          the query
 * @param score_in          the score of the alignment
 * @param numForbidden      number of forbidden ranges
 * @param forbiddenRanges   lists areas that should not be aligned
 * @param positionSpecific  determines whether matrix is position
 *                          specific or not
 */
static int
BLspecialSmithWatermanFindStart(int * score_out,
                                int *matchSeqStart, int *queryStart,
                                const Uint1 * matchSeq, int matchSeqLength,
                                const Uint1 *query, int **matrix,
                                int gapOpen, int gapExtend, int matchSeqEnd,
                                int queryEnd, int score_in,
                                const int *numForbidden,
                                int ** forbiddenRanges,
                                int positionSpecific)
{
    int bestScore;               /* best score seen so far */
    int newScore;                /* score of next entry */
    int bestMatchSeqPos, bestQueryPos; /* position starting best score in
                                          matchSeq and database sequences */
    SwGapInfo *scoreVector;      /* keeps one row of the
                                    Smith-Waterman matrix; overwrite
                                    old row with new row*/
    int *matrixRow;              /* one row of score matrix */
    int newGapCost;              /* cost to have a gap of one character */
    int prevScoreNoGapMatchSeq;  /* score one row and column up
                                    with no gaps*/
    int prevScoreGapMatchSeq;    /* score if a gap already started in
                                    matchSeq */
    int continueGapScore;        /* score for continuing a gap in query */
    int matchSeqPos, queryPos;   /* positions in matchSeq and query */
    int forbidden;               /* is this position forbidden? */
    int f;                       /* index over forbidden positions */
    
    scoreVector = (SwGapInfo *) malloc(matchSeqLength * sizeof(SwGapInfo));
    if (scoreVector == NULL) {
        return -1;
    }
    bestMatchSeqPos = 0;
    bestQueryPos = 0;
    bestScore = 0;
    newGapCost = gapOpen + gapExtend;
    for (matchSeqPos = 0;  matchSeqPos < matchSeqLength;  matchSeqPos++) {
        scoreVector[matchSeqPos].noGap = 0;
        scoreVector[matchSeqPos].gapExists = -(gapOpen);
    }
    for (queryPos = queryEnd;  queryPos >= 0;  queryPos--) {
        if (positionSpecific)
            matrixRow = matrix[queryPos];
        else
            matrixRow = matrix[query[queryPos]];
        newScore = 0;
        prevScoreNoGapMatchSeq = 0;
        prevScoreGapMatchSeq = -(gapOpen);
        for (matchSeqPos = matchSeqEnd;  matchSeqPos >= 0;  matchSeqPos--) {
            /* testing scores with a gap in matchSeq, either starting a
             * new gap or extending an existing gap*/
            if ((newScore = newScore - newGapCost) >
                (prevScoreGapMatchSeq = prevScoreGapMatchSeq - gapExtend))
                prevScoreGapMatchSeq = newScore;
            /* testing scores with a gap in query, either starting a
             * new gap or extending an existing gap*/
            if ((newScore = scoreVector[matchSeqPos].noGap - newGapCost) >
                (continueGapScore =
                 scoreVector[matchSeqPos].gapExists - gapExtend))
                continueGapScore = newScore;
            /* compute new score extending one position in matchSeq
             * and query */
            forbidden = FALSE;
            for (f = 0;  f < numForbidden[queryPos];  f++) {
                if ((matchSeqPos >= forbiddenRanges[queryPos][2 * f]) &&
                    (matchSeqPos <= forbiddenRanges[queryPos][2*f + 1])) {
                    forbidden = TRUE;
                    break;
                }
            }
            if (forbidden)
                newScore = COMPO_SCORE_MIN;
            else
                newScore =
                    prevScoreNoGapMatchSeq + matrixRow[matchSeq[matchSeqPos]];
            if (newScore < 0)
                newScore = 0; /* Smith-Waterman locality condition */
            /* test two alternatives */
            if (newScore < prevScoreGapMatchSeq)
                newScore = prevScoreGapMatchSeq;
            if (newScore < continueGapScore)
                newScore = continueGapScore;
            prevScoreNoGapMatchSeq = scoreVector[matchSeqPos].noGap;
            scoreVector[matchSeqPos].noGap = newScore;
            scoreVector[matchSeqPos].gapExists = continueGapScore;
            if (newScore > bestScore) {
                bestScore = newScore;
                bestQueryPos = queryPos;
                bestMatchSeqPos = matchSeqPos;
            }
            if (bestScore >= score_in)
                break;
        }
        if (bestScore >= score_in)
            break;
    }
    free(scoreVector);
    if (bestScore < 0)
        bestScore = 0;
    *matchSeqStart = bestMatchSeqPos;
    *queryStart = bestQueryPos;
    *score_out = bestScore;
    
    return 0;
}


/**
 * Release the storage associated with the fields of self, but do not
 * delete self
 *
 * @param self          an instance of Blast_ForbiddenRanges [in][out]
 */
void
Blast_ForbiddenRangesRelease(Blast_ForbiddenRanges * self)
{
    int f;
    if (self->ranges) {
        for (f = 0;  f < self->capacity;  f++) free(self->ranges[f]);
    }
    free(self->ranges);       self->ranges       = NULL;
    free(self->numForbidden); self->numForbidden = NULL;
}


/**
 * Initialize a new, empty Blast_ForbiddenRanges
 *
 * @param self              object to be initialized
 * @param capacity          the number of ranges that may be stored
 *                          (must be at least as long as the length
 *                           of the query)
 */
int
Blast_ForbiddenRangesInitialize(Blast_ForbiddenRanges * self,
                                int capacity)
{
    int f;
    self->capacity  = capacity;
    self->numForbidden = NULL;
    self->ranges       = NULL;
    self->isEmpty      = TRUE;

    self->numForbidden = (int *) calloc(capacity, sizeof(int));
    if (self->numForbidden == NULL)
        goto error_return;
    self->ranges       = (int **) calloc(capacity, sizeof(int *));
    if (self->ranges == NULL) 
        goto error_return;
    for (f = 0;  f < capacity;  f++) {
        self->numForbidden[f] = 0;
        self->ranges[f]       = (int *) malloc(2 * sizeof(int));
        if (self->ranges[f] == NULL) 
            goto error_return;
        self->ranges[f][0]    = 0;
        self->ranges[f][1]    = 0;
    }
    return 0;
error_return:
    Blast_ForbiddenRangesRelease(self);
    return -1;
}


/** Reset self to be empty */
void
Blast_ForbiddenRangesClear(Blast_ForbiddenRanges * self)
{
    int f;
    for (f = 0;  f < self->capacity;  f++) {
        self->numForbidden[f] = 0;
    }
    self->isEmpty = TRUE;
}


/** Add some ranges to self
 * @param self          an instance of Blast_ForbiddenRanges [in][out]
 * @param queryStart    start of the alignment in the query sequence
 * @param queryEnd      the end of the alignment in the query sequence
 * @param matchStart    start of the alignment in the subject sequence
 * @param matchEnd      the end of the alignment in the subject sequence
 */
int
Blast_ForbiddenRangesPush(Blast_ForbiddenRanges * self,
                          int queryStart,
                          int queryEnd,
                          int matchStart,
                          int matchEnd)
{
    int f;
    for (f = queryStart;  f < queryEnd;  f++) {
        int last = 2 * self->numForbidden[f];
        if (0 != last) {    /* we must resize the array */
            int * new_ranges =
                realloc(self->ranges[f], (last + 2) * sizeof(int));
            if (new_ranges == NULL) 
                return -1;
            self->ranges[f] = new_ranges;
        }
        self->ranges[f][last]     = matchStart;
        self->ranges[f][last + 1] = matchEnd;

        self->numForbidden[f]++;
    }
    self->isEmpty = FALSE;

    return 0;
}


/**
 * Calls BLbasicSmithWatermanScoreOnly if forbiddenRanges is empty and
 * calls BLspecialSmithWatermanScoreOnly otherwise.  See
 * BLspecialSmithWatermanScoreOnly for the meaning of the parameters
 * to this routine.
 */
int
Blast_SmithWatermanScoreOnly(int *score,
                             int *matchSeqEnd, int *queryEnd,
                             const Uint1 * subject_data, int subject_length,
                             const Uint1 * query_data, int query_length,
                             int **matrix,
                             int gapOpen,
                             int gapExtend,
                             int positionSpecific,
                             const Blast_ForbiddenRanges * forbiddenRanges )
{
    if (forbiddenRanges->isEmpty) {
        return BLbasicSmithWatermanScoreOnly(score, matchSeqEnd,
                                             queryEnd, subject_data,
                                             subject_length,
                                             query_data, query_length,
                                             matrix, gapOpen,
                                             gapExtend,
                                             positionSpecific);
    } else {
        return BLspecialSmithWatermanScoreOnly(score, matchSeqEnd,
                                               queryEnd, subject_data,
                                               subject_length,
                                               query_data,
                                               query_length, matrix,
                                               gapOpen, gapExtend,
                                               forbiddenRanges->numForbidden,
                                               forbiddenRanges->ranges,
                                               positionSpecific);
    }
}


/**
 * Calls BLSmithWatermanFindStart if forbiddenRanges is empty and
 * calls BLspecialSmithWatermanFindStart otherwise. See
 * BLspecialSmithWatermanFindStart for the meaning of the parameters
 * to this routine.
 */
int
Blast_SmithWatermanFindStart(int * score_out,
                             int *matchSeqStart,
                             int *queryStart,
                             const Uint1 * subject_data, int subject_length,
                             const Uint1 * query_data,
                             int **matrix,
                             int gapOpen,
                             int gapExtend,
                             int matchSeqEnd,
                             int queryEnd,
                             int score_in,
                             int positionSpecific,
                             const Blast_ForbiddenRanges * forbiddenRanges)
{
    if (forbiddenRanges->isEmpty) {
        return BLSmithWatermanFindStart(score_out, matchSeqStart,
                                        queryStart, subject_data,
                                        subject_length, query_data,
                                        matrix, gapOpen, gapExtend,
                                        matchSeqEnd, queryEnd,
                                        score_in, positionSpecific);
    } else {
        return BLspecialSmithWatermanFindStart(score_out,
                                               matchSeqStart,
                                               queryStart,
                                               subject_data,
                                               subject_length,
                                               query_data, matrix,
                                               gapOpen, gapExtend,
                                               matchSeqEnd, queryEnd,
                                               score_in,
                                               forbiddenRanges->numForbidden,
                                               forbiddenRanges->ranges,
                                               positionSpecific);
    }
}
