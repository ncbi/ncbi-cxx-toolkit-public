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

/** @file compo_heap.c
 * @author E. Michael Gertz, Alejandro Schaffer
 *
 * Defines a "heap" data structure that is used to store computed alignments
 * when composition adjustment of scoring matrices is used.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <assert.h>
#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/composition_adjustment/compo_heap.h>


/** Define COMPO_INTENSE_DEBUG to be true to turn on rigorous but
 * expensive consistency tests in the composition_adjustment
 * module.
 *
 * This macro is usually used as part of a C-conditional
 * if (COMPO_INTENSE_DEBUG) {
 *     perform expensive tests 
 * }
 * The C compiler will then validate the code to perform the tests, but
 * will almost always strip the code if COMPO_INTENSE_DEBUG is false.
 */
#ifndef COMPO_INTENSE_DEBUG
#define COMPO_INTENSE_DEBUG 0
#endif

/** The initial capacity of the heap will be set to the smaller of this
 * and the heap threshold */
#define HEAP_INITIAL_CAPACITY 100
/** When the heap is about to exceed its capacity, it will be grown by
 * the minimum of a multiplicative factor of HEAP_RESIZE_FACTOR
 * and an additive factor of HEAP_MIN_RESIZE. The heap never
 * decreases in size */
#define HEAP_RESIZE_FACTOR 1.5
/** @sa HEAP_RESIZE_FACTOR */
#define HEAP_MIN_RESIZE 100

/** Return -1/0/1 if a is less than/equal to/greater than b. */
#define CMP(a,b) ((a)>(b) ? 1 : ((a)<(b) ? -1 : 0))


/**
 * The struct BlastCompo_HeapRecord data type is used below to define
 * the internal structure of a BlastCompo_Heap (see below).  A
 * BlastCompo_HeapRecord represents all alignments of a query sequence
 * to a particular matching sequence.
 */
typedef struct BlastCompo_HeapRecord {
    double        bestEvalue;     /**< best (smallest) evalue of all
                                       alignments in the record */
    int           bestScore;      /**< best (largest) score; used to
                                       break ties between records with
                                       the same e-value */
    int           subject_index;  /**< index of the subject sequence in
                                       the database */
    void *        theseAlignments;  /**< a collection of alignments */
} BlastCompo_HeapRecord;


/** Compare two records in the heap.  */
static int
s_CompoHeapRecordCompare(BlastCompo_HeapRecord * place1,
                         BlastCompo_HeapRecord * place2)
{
    int result;
    if (0 == (result = CMP(place1->bestEvalue, place2->bestEvalue)) &&
        0 == (result = CMP(place2->bestScore, place1->bestScore))) {
        result = CMP(place1->subject_index, place2->subject_index);
    }
    return result > 0;
}


/** Swap two records in the heap. */
static void
s_CompoHeapRecordSwap(BlastCompo_HeapRecord * record1,
                      BlastCompo_HeapRecord * record2)
{
    /* bestEvalue, bestScore, theseAlignments and subject_index are temporary
     * variables used to perform the swap. */
    double bestEvalue;
    int bestScore, subject_index;
    void * theseAlignments;

    bestEvalue           = record1->bestEvalue;
    record1->bestEvalue  = record2->bestEvalue;
    record2->bestEvalue  = bestEvalue;

    bestScore            = record1->bestScore;
    record1->bestScore   = record2->bestScore;
    record2->bestScore   = bestScore;

    subject_index             = record1->subject_index;
    record1->subject_index    = record2->subject_index;
    record2->subject_index    = subject_index;

    theseAlignments           = record1->theseAlignments;
    record1->theseAlignments  = record2->theseAlignments;
    record2->theseAlignments  = theseAlignments;
}


/**
 * Verify that the subtree rooted at element i is ordered so as to be
 * as to be a valid heap.  This routine checks every element in the
 * subtree, and so is very time consuming.  It is for debugging
 * purposes only.
 */
static int
s_CompoHeapIsValid(BlastCompo_HeapRecord * heapArray, int i, int n)
{
    /* indices of nodes to the left and right of node i */
    int left = 2 * i, right = 2 * i + 1;

    if (right <= n) {
        return !s_CompoHeapRecordCompare(&(heapArray[right]),
                                         &(heapArray[i])) &&
            s_CompoHeapIsValid(heapArray, right, n);
    }
    if (left <= n) {
        return !s_CompoHeapRecordCompare(&(heapArray[left]),
                                         &(heapArray[i])) &&
            s_CompoHeapIsValid(heapArray, left, n);
    }
    return TRUE;
}


/**
 * Relocate the top element of a subtree so that on exit the subtree
 * is in valid heap order.  On entry, all elements but the root of the
 * subtree must be in valid heap order.
 *
 * @param heapArray    array representing the heap stored as a binary tree
 * @param top          the index of the root element of a subtree
 * @param n            the size of the entire heap.
 */
static void
s_CompoHeapifyDown(BlastCompo_HeapRecord * heapArray,
                       int top, int n)
{
    int i, left, right, largest;    /* placeholders for indices in swapping */

    largest = top;
    do {
        i = largest;
        left  = 2 * i;
        right = 2 * i + 1;
        if (left <= n &&
            s_CompoHeapRecordCompare(&heapArray[left],
                                     &heapArray[i])) {
            largest = left;
        } else {
            largest = i;
        }
        if (right <= n &&
            s_CompoHeapRecordCompare(&heapArray[right],
                                     &heapArray[largest])) {
            largest = right;
        }
        if (largest != i) {
            s_CompoHeapRecordSwap(&heapArray[i], &heapArray[largest]);
        }
    } while (largest != i);
    if (COMPO_INTENSE_DEBUG) {
        assert(s_CompoHeapIsValid(heapArray, top, n));
    }
}


/**
 * Relocate a leaf in the heap so that the entire heap is in valid
 * heap order.  On entry, all elements but the leaf must be in valid
 * heap order.
 *
 * @param heapArray      array representing the heap as a binary tree
 * @param i              element in heap array that may be out of order [in]
 */
static void
s_CompoHeapifyUp(BlastCompo_HeapRecord * heapArray, int i)
{
    int parent = i / 2;          /* index to the node that is the
                                    parent of node i */
    while (parent >= 1 && s_CompoHeapRecordCompare(&heapArray[i],
                                                   &heapArray[parent]))
    {
        s_CompoHeapRecordSwap(&heapArray[i], &heapArray[parent]);

        i       = parent;
        parent /= 2;
    }
    if (COMPO_INTENSE_DEBUG) {
        assert(s_CompoHeapIsValid(heapArray, 1, i));
    }
}


/** Convert a BlastCompo_Heap from a representation as an unordered array to
 *  a representation as a heap-ordered array.
 *
 *  @param self         the BlastCompo_Heap to convert
 */
static void
s_ConvertToHeap(BlastCompo_Heap * self)
{
    if (NULL != self->array) {    /* If we aren't already a heap */
        int i;                     /* heap node index */
        int n;                     /* number of elements in the heap */
        self->heapArray = self->array;
        self->array     = NULL;

        n = self->n;
        for (i = n / 2;  i >= 1;  --i) {
            s_CompoHeapifyDown(self->heapArray, i, n);
        }
    }
    if (COMPO_INTENSE_DEBUG) {
        assert(s_CompoHeapIsValid(self->heapArray, 1, self->n));
    }
}


/** Return true if self may insert a match that had the given eValue,
 * score and subject_index.
 *
 *  @param self           a BlastCompo_Heap
 *  @param eValue         the evalue to be tested.
 *  @param score          the score to be tested
 *  @param subject_index  the subject_index to be tested.
 */
int
BlastCompo_HeapWouldInsert(BlastCompo_Heap * self,
                           double eValue,
                           int score,
                           int subject_index)
{
    if (self->n < self->heapThreshold ||
        eValue <= self->ecutoff ||
        eValue <  self->worstEvalue) {
        return TRUE;
    } else {
        /* self is either currently a heap, or must be converted to
         * one; use s_CompoHeapRecordCompare to compare against
         * the worst element in the heap */
        BlastCompo_HeapRecord heapRecord; /* temporary record to
                                             compare against */
        if (self->heapArray == NULL) s_ConvertToHeap(self);

        heapRecord.bestEvalue       = eValue;
        heapRecord.bestScore        = score;
        heapRecord.subject_index    = subject_index;
        heapRecord.theseAlignments  = NULL;

        return s_CompoHeapRecordCompare(&self->heapArray[1], &heapRecord);
    }
}


/**
 * Insert a new heap record at the end of *array, possibly resizing
 * the array to hold the new record.
 *
 * @param *array            the array to receive the new record
 * @param *length           number of records already in *array
 * @param *capacity         allocated size of *array
 * @param alignments        a list of alignments
 * @param eValue            the best evalue among the alignments
 * @param score             the best score among the alignments
 * @param subject_index     the index of the subject sequence in the database
 * @return 0 on success, -1 on failure (out-of-memory)
 */
static int
s_CompHeapRecordInsertAtEnd(BlastCompo_HeapRecord **array,
                            int * length,
                            int * capacity,
                            void * alignments,
                            double eValue,
                            int score,
                            int subject_index)
{
    BlastCompo_HeapRecord *heapRecord;    /* destination for the new
                                             alignments */
    if (*length >= *capacity) {
        /* The destination array must be resized */
        int new_capacity;         /* capacity the resized heap */
        BlastCompo_HeapRecord * new_array; 

        new_capacity      = MAX(HEAP_MIN_RESIZE + *capacity,
                                (int) (HEAP_RESIZE_FACTOR * (*capacity)));
        new_array = realloc(*array, (new_capacity + 1) *
                            sizeof(BlastCompo_HeapRecord));
        if (new_array == NULL) {   /* out of memory */
            return -1;
        }
        *array      = new_array;
        *capacity   = new_capacity;
    }
    heapRecord                  = &(*array)[++(*length)];
    heapRecord->bestEvalue      = eValue;
    heapRecord->bestScore       = score;
    heapRecord->theseAlignments = alignments;
    heapRecord->subject_index   = subject_index;

    return 0;
}


/**
 * Try to insert a collection of alignments into a heap.
 *
 * @param self              the heap
 * @param alignments        a collection of alignments, in an unspecified
 *                          format
 * @param eValue            the best evalue among the alignments
 * @param score             the best score among the alignments
 * @param subject_index     the index of the subject sequence in the database
 * @param discardedAlignments  a collection of alignments that must be
 *                             deleted (passed back to the calling routine
 *                             as this routine does know how to delete them)
 * @return 0 on success,  -1 for out of memory */
int
BlastCompo_HeapInsert(BlastCompo_Heap * self,
                      void * alignments,
                      double eValue,
                      int score,
                      int subject_index,
                      void ** discardedAlignments)
{
    *discardedAlignments = NULL;
    if (self->array && self->n >= self->heapThreshold) {
        s_ConvertToHeap(self);
    }
    if (self->array != NULL) {
        /* "self" is currently a list. Add the new alignments to the end */
        int status = 
            s_CompHeapRecordInsertAtEnd(&self->array, &self->n,
                                        &self->capacity, alignments,
                                        eValue, score,
                                        subject_index);
        if (status != 0) { /* out of memory */
            return -1;
        }
        if (self->worstEvalue < eValue) {
            self->worstEvalue = eValue;
        }
    } else {                      /* "self" is currently a heap */
        if (self->n < self->heapThreshold ||
            (eValue <= self->ecutoff &&
             self->worstEvalue <= self->ecutoff)) {
            /* The new alignments must be inserted into the heap, and all old
             * alignments retained */
            int status =
                s_CompHeapRecordInsertAtEnd(&self->heapArray,
                                            &self->n,
                                            &self->capacity,
                                            alignments, eValue,
                                            score, subject_index);
            if (status != 0) { /* out of memory */
                return -1;
            }
            s_CompoHeapifyUp(self->heapArray, self->n);
        } else {
            /* Some set of alignments must be discarded; discardedAlignments
             * will hold a pointer to these alignments. */
            BlastCompo_HeapRecord heapRecord;   /* Candidate record
                                                   for insertion */
            heapRecord.bestEvalue      = eValue;
            heapRecord.bestScore       = score;
            heapRecord.theseAlignments = alignments;
            heapRecord.subject_index   = subject_index;

            if (s_CompoHeapRecordCompare(&self->heapArray[1],
                                             &heapRecord)) {
                /* The new record should be inserted, and the largest
                 * element currently in the heap may be discarded */
                *discardedAlignments = self->heapArray[1].theseAlignments;
                memcpy(&self->heapArray[1], &heapRecord,
                       sizeof(BlastCompo_HeapRecord));
            } else {
                *discardedAlignments = heapRecord.theseAlignments;
            }
            s_CompoHeapifyDown(self->heapArray, 1, self->n);
        }
        /* end else some set of alignments must be discarded */
        self->worstEvalue = self->heapArray[1].bestEvalue;
        if (COMPO_INTENSE_DEBUG) {
            assert(s_CompoHeapIsValid(self->heapArray, 1, self->n));
        }
    }
    /* end else "self" is currently a heap. */
    return 0;  /* success */
}


/**
 * Return true if only matches with evalue <= self->ecutoff may be
 * inserted.
 *
 * @param self          a BlastCompo_Heap
 */
int
BlastCompo_HeapFilledToCutoff(const BlastCompo_Heap * self)
{
    return self->n >= self->heapThreshold &&
        self->worstEvalue <= self->ecutoff;
}


/** Initialize a new BlastCompo_Heap; parameters to this function correspond
 * directly to fields in the BlastCompo_Heap 
 *
 * @return 0 on success,  -1 for out of memory */
int
BlastCompo_HeapInitialize(BlastCompo_Heap * self, int heapThreshold,
                          double ecutoff)
{
    self->n             = 0;
    self->heapThreshold = heapThreshold;
    self->ecutoff       = ecutoff;
    self->heapArray     = NULL;
    self->capacity      = MIN(HEAP_INITIAL_CAPACITY, heapThreshold);
    self->worstEvalue   = 0;
    /* Begin life as a list */
    self->array = calloc(self->capacity + 1, sizeof(BlastCompo_HeapRecord));
    
    return self->array != NULL ? 0 : -1;
}


/**
 * Release the storage associated with the fields of a BlastCompo_Heap. Don't
 * delete the BlastCompo_Heap structure itself.
 *
 * @param self          BlastCompo_Heap whose storage will be released
 */
void
BlastCompo_HeapRelease(BlastCompo_Heap * self)
{
    if (self->heapArray) free(self->heapArray);
    if (self->array) free(self->array);

    self->n = self->capacity = self->heapThreshold = 0;
    self->heapArray = NULL;  self->array = NULL;
}


/**
 * Remove and return the element in the BlastCompo_Heap with largest
 * (worst) evalue; ties are broken according to the order specified
 * by the s_CompoHeapRecordCompare routine.
 *
 * @param self           a BlastCompo_Heap
 */
void *
BlastCompo_HeapPop(BlastCompo_Heap * self)
{
    void * results = NULL;   /* the list of SeqAligns to be returned */

    s_ConvertToHeap(self);
    if (self->n > 0) { /* The heap is not empty */
        BlastCompo_HeapRecord *first, *last; /* The first and last
                                                elements of the array
                                                that represents the
                                                heap.  */
        first = &self->heapArray[1];
        last  = &self->heapArray[self->n];

        results = first->theseAlignments;
        if (--self->n > 0) {
            /* The heap is still not empty */
            memcpy(first, last, sizeof(BlastCompo_HeapRecord));
            s_CompoHeapifyDown(self->heapArray, 1, self->n);
        }
    }
    if (COMPO_INTENSE_DEBUG) {
        assert(s_CompoHeapIsValid(self->heapArray, 1, self->n));
    }
    return results;
}
