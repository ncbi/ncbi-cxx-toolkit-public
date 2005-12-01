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

/** @file compo_heap.h
 * @author Alejandro Schaffer, E. Michael Gertz
 *
 * Declares a "heap" data structure that is used to store computed alignments
 * when composition adjustment of scoring matrices is used.
 */

#ifndef __COMPO_HEAP__
#define __COMPO_HEAP__

#include <algo/blast/core/blast_export.h>

#ifdef __cplusplus
extern "C" {
#endif

struct BlastCompo_HeapRecord;

/**
 * A BlastCompo_Heap represents a collection of alignments between one
 * query sequence and several matching subject sequences.
 *
 * Each matching sequence is allocated one record in a
 * BlastCompo_Heap.  The eValue of a query-subject pair is the best
 * (smallest positive) evalue of all alignments between the two
 * sequences.
 *
 * The comparison function for matches is BlastCompo_HeapRecordCompare.  A
 * match will be inserted in the the BlastCompo_Heap if:
 * - there are fewer that BlastCompo_Heap::heapThreshold elements in
 *   the BlastCompo_Heap;
 * - the eValue of the match is <= BlastCompo_Heap::ecutoff; or
 * - the match is less than (as determined by BlastCompo_HeapRecordCompare) the
 *   largest (worst) match already in the BlastCompo_Heap.
 *
 * If there are >= BlastCompo_Heap::heapThreshold matches already in
 * the BlastCompo_Heap when a new match is to be inserted, then the
 * largest match (as determined by BlastCompo_HeapRecordCompare) is
 * removed, unless the eValue of the largest match <=
 * BlastCompo_Heap::ecutoff.  Matches with eValue <=
 * BlastCompo_Heap::ecutoff are never removed by the insertion
 * routine.  As a consequence, the BlastCompo_Heap can hold an
 * arbitrarily large number of matches, although it is atypical for
 * the number of matches to be greater than
 * BlastCompo_Heap::heapThreshold.
 *
 * Once all matches have been collected, the BlastCompo_HeapPop
 * routine may be invoked to return all alignments in order.
 *
 * While the number of elements in a heap < BlastCompo_Heap::heapThreshold,
 * the BlastCompo_Heap is implemented as an unordered array, rather
 * than a heap-ordered array.  The BlastCompo_Heap is converted to a
 * heap-ordered array as soon as it becomes necessary to order the
 * matches by evalue.  The routines that operate on a BlastCompo_Heap
 * should behave properly whichever state the BlastCompo_Heap is in.
 */
typedef struct BlastCompo_Heap {
    int n;                     /**< The current number of elements */
    int capacity;              /**< The maximum number of elements
                                    that may be inserted before the
                                    BlastCompo_Heap must be resized, this
                                    number must be >= heapThreshold */
    int heapThreshold;          /**< see above */
    double ecutoff;             /**< matches with evalue below ecutoff may
                                    always be inserted in the BlastCompo_Heap */
    double worstEvalue;         /**< the worst (biggest) evalue currently in
                                    the heap */

    struct BlastCompo_HeapRecord *array;
    struct BlastCompo_HeapRecord *heapArray;
} BlastCompo_Heap;


NCBI_XBLAST_EXPORT
int BlastCompo_HeapWouldInsert(BlastCompo_Heap * self, double eValue,
                               int score, int subject_index);
NCBI_XBLAST_EXPORT
int BlastCompo_HeapInsert(BlastCompo_Heap * self, void * alignments,
                           double eValue, int score, int
                           subject_index, void ** discardedAligns);

NCBI_XBLAST_EXPORT
int BlastCompo_HeapFilledToCutoff(const BlastCompo_Heap * self);

NCBI_XBLAST_EXPORT
int BlastCompo_HeapInitialize(BlastCompo_Heap * self, int heapThreshold,
                              double ecutoff);

NCBI_XBLAST_EXPORT
void BlastCompo_HeapRelease(BlastCompo_Heap * self);

NCBI_XBLAST_EXPORT
void * BlastCompo_HeapPop(BlastCompo_Heap * self);

#ifdef __cplusplus
}
#endif

#endif
