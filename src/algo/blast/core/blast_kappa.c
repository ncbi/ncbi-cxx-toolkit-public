/* $Id$ 
 * ==========================================================================
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
 * Authors: Alejandro Schaffer, Mike Gertz (ported to algo/blast by Tom Madden)
 *
 */

/** @file blast_kappa.c
 * Utilities for doing Smith-Waterman alignments and adjusting the scoring 
 * system for each match in blastpgp
 */

static char const rcsid[] = 
    "$Id$";

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_kappa.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/blast_filter.h>
#include "blast_psi_priv.h"
#include "matrix_freq_ratios.h"
#include "blast_gapalign_pri.h"



#define EVALUE_STRETCH 5 /*by what factor might initially reported E-value
                           exceed true Evalue*/

#define PRO_TRUE_ALPHABET_SIZE 20
#define scoreRange 10000

#define XCHAR   21    /*character for low-complexity columns*/
#define STARCHAR   25    /*character for stop codons*/




/*positions of true characters in protein alphabet*/
Int4 trueCharPositions[20] = {1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,22};


/** Structure used for full Smith-Waterman results. 
*/
typedef struct SWResults {
    struct SWResults *next;    /**< next object in list */
    Uint1* seq;	               /**< match sequence. */
    Int4 seqStart;             /**< start of alignment on match */
    Int4 seqEnd;               /**< end of alignment on match */
    Int4 queryStart;           /**< start of alignment on query */
    Int4 queryEnd;             /**< end of alignment on query */
    Int4 score;                /**< score of alignment */
    double eValue;             /**< best expect value for this match record */
    double eValueThisAlign;    /**< expect value of this alignment. */
    double Lambda;             /**< Karlin-Altschul parameter. */
    double logK;               /**< log of Karlin-Altschul parameter */
    Boolean isFirstAlignment;  /**< TRUE if first alignment for this sequence */
    Int4 subject_index;        /**< ordinal ID of match sequence, needed to break 
                                  ties on rare occasions */
    BlastHSP* hsp;             /**< Saves alignment informaiton for conversion to SeqAlign. */
} SWResults;

/**
 * Frees the linked-list of SWResults.  Does not deallocate the BlastHSP 
 * on the SWResults as that is saved elsewhere.
 * @param sw_results the head of the linked list to be freed [in]
 * @return NULL pointer 
*/
static SWResults* SWResultsFree(SWResults* sw_results)
{
    SWResults *current, *next;

    next = current = sw_results;
    while (current)
    {
       next = current->next;
       sfree(current);
       current = next;
    }

    return NULL;
}

/**
 * SWResultsNew Create a new instance of the SWResults struct, initializing
 *              it with values common to different kinds of searches 
 *              The parameters of this function correspond directly to fields
 *              in the SWResults data structure.
 * @param sequence match sequence [in]
 * @param score score of match [in]
 * @param newEvalue expect value of this alignment [in]
 * @param bestEvalue lowest expect value of this match sequence [in]
 * @param isFirstAlignment TRUE if first alignment for this sequence [in]
 * @param lambda Karlin-Altschul parameter [in]
 * @param logK log of Karlin-Altschul parameter [in]
 * @param subject_index ordinal ID of match sequence [in]
 */
static SWResults *
SWResultsNew(Uint1* sequence,
             Int4 score,
             double newEvalue,
             double bestEvalue,
             Boolean isFirstAlignment,
             double lambda,
             double logK,
             Int4 subject_index)
{
  SWResults *newSW;             /* The newly created instance of SWResults */

  newSW = (SWResults *) calloc(1, sizeof(SWResults));
  if(newSW) {
    newSW->seq     = sequence;
    newSW->score   = score;
    newSW->eValue  = bestEvalue;
    newSW->Lambda  = lambda;
    newSW->logK    = logK;

    newSW->eValueThisAlign  = newEvalue;
    newSW->isFirstAlignment = isFirstAlignment;
    newSW->subject_index    = subject_index;

    newSW->next = NULL;
  }
  return newSW;
}


/**
 * An instance of struct Kappa_MatchRecord represents all alignments
 * of a query sequence to a matching subject sequence.
 *
 * For a given query-subject pair, a Kappa_MatchRecord is created once it
 * is known that the eValue of the best alignment is small enough to be 
 * significant.  Then alignments of the two sequences are added to the
 * Kappa_MatchRecord one at a time, using one of the following two routines
 * 
 * - Kappa_MatchRecordInsertHSP inserts the alignment represented
 *   by a single HSP into the match record.
 * - Kappa_MatchRecordInsertSwAlign inserts an alignment computed by
 *   the Smith-Waterman algorithm into the match record.
 * 
 * Alignments should be specified in order of smallest (best) e-value to
 * largest (worst) e-value.
 *
 * The Kappa_MatchRecord::alignments field stores the alignments in
 * the reverse order, i.e. from largest (worst) e-value to smallest
 * (best) e-value.  The reason the alignments are stored in reverse
 * order is that this order is consistent with the order that matches
 * are returned by a SWheap (see below), i.e. worst to best. 
 */ 

struct Kappa_MatchRecord {
  double  eValue;          /**< best evalue of all alignments the record */
  Int4  score;           /**< best score of all alignments the record */  
  Uint1*     sequence;        /**< the subject sequence */
  Int4         subject_index;   /**< the index number of the subject sequence */
  SWResults   *alignments;      /**< a list of query-subject alignments */
};
typedef struct Kappa_MatchRecord Kappa_MatchRecord;


/** Initialize a Kappa_MatchRecord.  Parameters to this function correspond
 *    directly to fields of Kappa_MatchRecord. 
 * @param self the record to be modified [in][out]
 * @param eValue expect value of this alignment [in]
 * @param score score of match [in]
 * @param sequence match sequence [in]
 * @param subject_index ordinal ID of sequence in database [in]
 */
static void
Kappa_MatchRecordInitialize(Kappa_MatchRecord * self,
                            double eValue,
                            Int4 score,
                            Uint1* sequence,
                            Int4 subject_index)
{
  self->eValue   = eValue;
  self->score    = score;
  self->sequence = sequence;
  self->subject_index = subject_index;
  self->alignments    = NULL;
}


/** The following procedure computes the number of identities in an
 *    alignment of query_seq to the matching sequence stored in
 *    SWAlign. The alignment is encoded in gap_info
 * @param SWAlign input structure holding HSP to be modified [in][out]
 * @param query_seq Query sequence used for calculation [in]
 */
static Int2 SWAlignGetNumIdentical(SWResults *SWAlign, Uint1* query_seq)
{
   Int4 num_ident; /*number of identities to return*/
   Int4 align_length; /*aligned length, calculated but discarded. */

   Blast_HSPGetNumIdentities(query_seq, SWAlign->seq, 
      SWAlign->hsp, TRUE, &num_ident, &align_length);
   
   SWAlign->hsp->num_ident = num_ident;
   return 0;
}

/**  
 * Insert an alignment represented by a seqAlign into the match
 *    record.
 * @param self the match record to be modified [in][out]
 * @param hsp contains alignment and scoring information, 
 *    will be NULLed out [in][out]
 * @param lambda a statistical parameter used to evaluate the significance of the
 *    match [in]
 * @param logK a statistical parameter used to evaluate the significance of the
 *    match [in]
 * @param localScalingFactor the factor by which the scoring system has been
 *    scaled in order to obtain greater precision [in]
 * @param query_seq Used to calculate percent identity [in]
 */
static void
Kappa_MatchRecordInsertHSP(
  Kappa_MatchRecord * self,     
  BlastHSP* *hsp,         
  double lambda,           
  double logK,             
  double localScalingFactor,
  Uint1* query_seq
) {
  SWResults *newSW;             /* A new SWResults object that
                                   represents the alignment to be
                                   inserted */

  newSW =
    SWResultsNew(self->sequence, self->score, 
                 (*hsp)->evalue, self->eValue, (Boolean) (NULL == self->alignments),
                 localScalingFactor * lambda, logK,
                 self->subject_index);

  newSW->queryStart = (*hsp)->gap_info->start1;
  newSW->seqStart   = (*hsp)->gap_info->start2;
  newSW->hsp   = *hsp;
  *hsp = NULL; /* Information stored on SWResults now. */
  SWAlignGetNumIdentical(newSW, query_seq); /* Calculate num identities, attach to HSP. */
  newSW->next       = self->alignments;

  self->alignments = newSW;
}


/**  
 * Insert an alignment computed by the Smith-Waterman algorithm into
 *    the match record.
 * @param self the match record to be modified [in][out]
 * @param newScore the score of the alignment [in]
 * @param newEvalue the expect value of the alignment [in]
 * @param lambda a statistical parameter used to evaluate the significance of the
 *    match [in]
 * @param logK a statistical parameter used to evaluate the significance of the
 *    match [in]
 * @param localScalingFactor the factor by which the scoring system has been
 *    scaled in order to obtain greater precision [in]
 * @param matchStart start of the alignment in the subject [in]
 * @param matchAlignmentExtent length of the alignment in the subject [in]
 * @param queryStart start of the alignment in the query [in]
 * @param queryAlignmentExtent length of the alignment in the query [in]
 * @param reverseAlignScript Alignment information (script) returned by 
 *    the X-drop alignment algorithm [in]
 * @param query_seq Used to calculate percent identity [in]
 */
static void
Kappa_MatchRecordInsertSwAlign(
  Kappa_MatchRecord * self,     
  Int4 newScore,         
  double newEvalue,        
  double lambda,           
  double logK,
  double localScalingFactor,
  Int4 matchStart,
  Int4 matchAlignmentExtent,
  Int4 queryStart,
  Int4 queryAlignmentExtent,
  Int4 * reverseAlignScript,
  Uint1* query_seq
) {
  SWResults *newSW;             /* A new SWResults object that
                                   represents the alignment to be
                                   inserted */
  GapEditBlock* editBlock=NULL; /* Contains representation of traceback. */

  if(NULL == self->alignments) {
    /* This is the first sequence recorded for this match. Use the x-drop
     * score, "newScore", as the score for the sequence */
    self->score = newScore;
  }
  newSW =
    SWResultsNew(self->sequence, self->score, newEvalue,
                 self->eValue, (Boolean) (NULL == self->alignments),
                 lambda * localScalingFactor, logK, self->subject_index);

  newSW->seqStart   = matchStart;
  newSW->seqEnd     = matchStart + matchAlignmentExtent;
  newSW->queryStart = queryStart;
  newSW->queryEnd   = queryStart + queryAlignmentExtent;
  newSW->next       = self->alignments;

  BLAST_TracebackToGapEditBlock(reverseAlignScript, queryAlignmentExtent, matchAlignmentExtent, 
     queryStart, matchStart, &editBlock);
 
  Blast_HSPInit(queryStart, queryStart + queryAlignmentExtent,
                matchStart, matchStart + matchAlignmentExtent,
                0, 0, 0, 0, newScore, &editBlock, &(newSW->hsp));
  newSW->hsp->evalue = newEvalue;
 
  SWAlignGetNumIdentical(newSW, query_seq); /* Calculate num identities, attach to HSP. */

  self->alignments  = newSW;
}


/**
 * The struct SWheapRecord data type is used below to define the
 * internal structure of a SWheap (see below).  A SWheapRecord
 * represents all alignments of a query sequence to a particular
 * matching sequence.
 *
 * The SWResults::theseAlignments field is a linked list of alignments
 * of the query-subject pair.  The list is ordered by evalue in
 * descending order. Thus the first element has biggest (worst) evalue
 * and the last element has smallest (best) evalue. 
 */
typedef struct SWheapRecord {
  double bestEvalue;       /* best (smallest) evalue of all alignments
                                 * in the record */
  SWResults *theseAlignments;   /* a list of alignments */
} SWheapRecord;


/**  Compare two records in the heap.  
 * @param place1 the first record to be compared [in]
 * @param place2 the other record to be compared [in]
 */
static Boolean
SWheapRecordCompare(SWheapRecord * place1,
                    SWheapRecord * place2)
{
  return ((place1->bestEvalue > place2->bestEvalue) ||
          (place1->bestEvalue == place2->bestEvalue &&
           place1->theseAlignments->subject_index >
           place2->theseAlignments->subject_index));
}


/**  swap two records in the heap
 * @param heapArray holds the records to be swapped [in][out]
 * @param i the first record to be swapped [in]
 * @param j the other record to be swapped [in]
 */
static void
SWheapRecordSwap(SWheapRecord * heapArray,
                 Int4 i,
                 Int4 j)
{
  /* bestEvalue and theseAlignments are temporary variables used to
   * perform the swap. */
  double bestEvalue       = heapArray[i].bestEvalue;
  SWResults *theseAlignments   = heapArray[i].theseAlignments;

  heapArray[i].bestEvalue      = heapArray[j].bestEvalue;
  heapArray[i].theseAlignments = heapArray[j].theseAlignments;

  heapArray[j].bestEvalue      = bestEvalue;
  heapArray[j].theseAlignments = theseAlignments;
}


#ifdef KAPPA_INTENSE_DEBUG

/**
 * Verifies that the array heapArray[i] .. heapArray[n] is ordered so
 * as to be a valid heap.  This routine checks every element in the array,
 * an so is very time consuming.  It is for debugging purposes only.
 */
static Boolean
SWheapIsValid(SWheapRecord * heapArray,
              Int4 i,
              Int4 n)
{
  /* indices of nodes to the left and right of node i */
  Int4 left = 2 * i, right = 2 * i + 1;        

  if(right <= n) {
    return !SWheapRecordCompare(&(heapArray[right]), &(heapArray[i])) &&
      SWheapIsValid(heapArray, right, n);
  }
  if(left <= n) {
    return !SWheapRecordCompare(&(heapArray[left]), &(heapArray[i])) &&
      SWheapIsValid(heapArray, left, n);
  }
  return TRUE;
}

#define KAPPA_ASSERT(expr) ((expr) ? 0 : \
(fprintf( stderr, "KAPPA_ASSERT failed line %d: %s", __LINE__, #expr ), \
exit(1)))
#else
#define KAPPA_ASSERT(expr) (void)(0)
#endif


/** On entry, all but the first element of the array heapArray[i]
 * .. heapArray[n] are in valid heap order.  This routine rearranges
 * the elements so that on exit they all are in heap order. 
 * @param heapArray holds the heap [in][out]
 * @param i ?? [in]
 * @param n ?? [in]
 */
static void
SWheapifyDown(SWheapRecord * heapArray,
              Int4 i,
              Int4 n)
{
  Boolean moreswap = TRUE;      /* is more swapping needed */
  Int4 left, right, largest;    /* placeholders for indices in swapping */
  do {
    left  = 2 * i;
    right = 2 * i + 1;
    if((left <= n) &&
       (SWheapRecordCompare(&(heapArray[left]), &(heapArray[i]))))
      largest = left;
    else
      largest = i;
    if((right <= n) &&
       (SWheapRecordCompare(&(heapArray[right]), &(heapArray[largest]))))
      largest  = right;
    if(largest != i) {
      SWheapRecordSwap(heapArray, i, largest);
      /* push largest up the heap */
      i       = largest;       /* check next level down */
    } else
      moreswap = FALSE;
  } while(moreswap);            /* function builds the heap */
  KAPPA_ASSERT(SWheapIsValid(heapArray, i, n));
}


/** On entry, all but the last element of the array heapArray[i]
 *   .. heapArray[n] are in valid heap order.  This routine rearranges
 *   the elements so that on exit they all are in heap order.
 * @param heapArray holds the heap [in][out]
 * @param i the largest element to work with [in]
 * @param n the largest element in the heap [in]
 */
static void
SWheapifyUp(SWheapRecord * heapArray,
            Int4 i,
            Int4 n)
{
  Int4 parent = i / 2;          /* index to the node that is the
                                   parent of node i */
  while(parent >= 1 &&
        SWheapRecordCompare(&(heapArray[i]), &(heapArray[parent]))){
    SWheapRecordSwap(heapArray, i, parent);

    i       = parent;
    parent /= 2;
  }
  KAPPA_ASSERT(SWheapIsValid(heapArray, 1, n));
}

/** A SWheap represents a collection of alignments between one query
 * sequence and several matching subject sequences.  
 *
 * Each matching sequence is allocated one record in a SWheap.  The
 * eValue of a query-subject pair is the best (smallest positive)
 * evalue of all alignments between the two sequences.
 * 
 * A match will be inserted in the the SWheap if:
 * - there are fewer that SWheap::heapThreshold elements in the SWheap;
 * - the eValue of the match is <= SWheap::ecutoff; or
 * - the eValue of the match is less than the largest (worst) eValue
 *   already in the SWheap.
 *
 * If there are >= SWheap::heapThreshold matches already in the SWheap
 * when a new match is to be inserted, then the match with the largest
 * (worst) eValue is removed, unless the largest eValue <=
 * SWheap::ecutoff.  Matches with eValue <= SWheap::ecutoff are never
 * removed by the insertion routine.  As a consequence, the SWheap can
 * hold an arbitrarily large number of matches, although it is
 * atypical for the number of matches to be greater than
 * SWheap::heapThreshold.
 *
 * Once all matches have been collected, the SWheapToFlatList routine
 * may be invoked to return a list of all alignments. (see below).
 *
 * While the number of elements in a heap < SWheap::heapThreshold, the
 * SWheap is implemented as an unordered array, rather than a
 * heap-ordered array.  The SWheap is converted to a heap-ordered
 * array as soon as it becomes necessary to order the matches by
 * evalue.  The routines that operate on a SWheap should behave
 * properly whichever state the SWheap is in.
 */
struct SWheap {
  Int4 n;                       /**< The current number of elements */
  Int4 capacity;                /**< The maximum number of elements that may be 
                                   inserted before the SWheap must be resized */
  Int4 heapThreshold;           /**< see above */
  double ecutoff;          /**< matches with evalue below ecutoff may
                                   always be inserted in the SWheap */
  double worstEvalue;      /**< the worst (biggest) evalue currently in
                                   the heap */

  SWheapRecord *array;          /**< the SWheapRecord array if the SWheap is
                                   being represented as an unordered array */
  SWheapRecord *heapArray;      /**< the SWheapRecord array if the SWheap is
                                   being represented as an heap-ordered
                                   array. At least one of (array, heapArray)
                                   is NULL */

};
typedef struct SWheap SWheap;


/** Convert a SWheap from a representation as an unordered array to
 *    a representation as a heap-ordered array. 
 * @param self record to be modified [in][out]
 */
static void
ConvertToHeap(SWheap * self)
{
  if(NULL != self->array) {
    Int4 i;                     /* heap node index */
    Int4 n;                     /* number of elements in the heap */
    /* We aren't already a heap */
    self->heapArray = self->array;
    self->array     = NULL;

    n = self->n;
    for(i = n / 2; i >= 1; --i) {
      SWheapifyDown(self->heapArray, i, n);
    }
  }
  KAPPA_ASSERT(SWheapIsValid(self->heapArray, 1, self->n));
}

/*When the heap is about to exceed its capacity, it will be grown by
 *the minimum of a multiplicative factor of SWHEAP_RESIZE_FACTOR
 *and an additive factor of SWHEAP_MIN_RESIZE. The heap never
 *decreases in size */
#define SWHEAP_RESIZE_FACTOR 1.5
#define SWHEAP_MIN_RESIZE 100

/** Return true if self would insert a match that had the given eValue 
 * @param self record to be modified [in][out]
 * @param eValue specified expect value [in]
 */
static Boolean
SWheapWouldInsert(SWheap * self,
                  double eValue)
{
  return self->n < self->heapThreshold ||
    eValue <= self->ecutoff ||
    eValue < self->worstEvalue;
}


/** Try to insert matchRecord into the SWheap. The alignments stored in
 * matchRecord are used directly, i.e. they are not copied, but are
 * rather stored in the SWheap or deleted
 * @param self record to be modified [in][out]
 * @param matchRecord record to be inserted [in]
 */
static void
SWheapInsert(SWheap * self,
             Kappa_MatchRecord * matchRecord)
{
  if(self->array && self->n >= self->heapThreshold) {
    ConvertToHeap(self);
  }
  if(self->array != NULL) {
    /* "self" is currently a list. Add the new alignments to the end */
    SWheapRecord *heapRecord;   /* destination for the new alignments */ 
    heapRecord                  = &self->array[++self->n];
    heapRecord->bestEvalue      = matchRecord->eValue;
    heapRecord->theseAlignments = matchRecord->alignments;
    if( self->worstEvalue < matchRecord->eValue ) {
      self->worstEvalue = matchRecord->eValue;
    }
  } else {                      /* "self" is currently a heap */
    if(self->n < self->heapThreshold ||
       (matchRecord->eValue <= self->ecutoff &&
        self->worstEvalue <= self->ecutoff)) {
      SWheapRecord *heapRecord; /* Destination for the new alignments */
      /* The new alignments must be inserted into the heap, and all old
       * alignments retained */
      if(self->n >= self->capacity) {
        /* The heap must be resized */
        Int4 newCapacity;       /* capacity the heap will have after
                                 * it is resized */
        newCapacity      = MAX(SWHEAP_MIN_RESIZE + self->capacity,
                               (Int4) (SWHEAP_RESIZE_FACTOR * self->capacity));
        self->heapArray  = (SWheapRecord *)
          realloc(self->heapArray, (newCapacity + 1) * sizeof(SWheapRecord));
        self->capacity   = newCapacity;
      }
      /* end if the heap must be resized */
      heapRecord    = &self->heapArray[++self->n];
      heapRecord->bestEvalue      = matchRecord->eValue;
      heapRecord->theseAlignments = matchRecord->alignments;

      SWheapifyUp(self->heapArray, self->n, self->n);
    } else {
      /* Some set of alignments must be discarded */
      SWResults *discardedAlignments = NULL;      /* alignments that
                                                   * will be discarded
                                                   * so that the new
                                                   * alignments may be
                                                   * inserted. */

      if(matchRecord->eValue >= self->worstEvalue) {
        /* the new alignments must be discarded */
        discardedAlignments = matchRecord->alignments;
      } else {
        /* the largest element in the heap must be discarded */
        SWheapRecord *heapRecord;     /* destination for the new alignments */
        discardedAlignments         = self->heapArray[1].theseAlignments;

        heapRecord                  = &self->heapArray[1];
        heapRecord->bestEvalue      = matchRecord->eValue;
        heapRecord->theseAlignments = matchRecord->alignments;

        SWheapifyDown(self->heapArray, 1, self->n);
      }
      /* end else the largest element in the heap must be discarded */
      while(discardedAlignments != NULL) {
        /* There are discarded alignments that have not been freed */
        SWResults *thisAlignment;     /* the head of the list of
                                       * discarded alignments */
        thisAlignment        = discardedAlignments;
        discardedAlignments  = thisAlignment->next;
        sfree(thisAlignment);
      }
      /* end while there are discarded alignments that have not been freed */
    } 
    /* end else some set of alignments must be discarded */
    
    self->worstEvalue = self->heapArray[1].bestEvalue;
    KAPPA_ASSERT(SWheapIsValid(self->heapArray, 1, self->n));
  }
  /* end else "self" is currently a heap. */

  /* The matchRecord->alignments pointer is no longer valid */
  matchRecord->alignments = NULL;
}


/** Return true if only matches with evalue <= self->ecutoff 
 *  may be inserted. 
 * @param self heap containing data [in]
 */
static Boolean
SWheapWillAcceptOnlyBelowCutoff(SWheap * self)
{
  return self->n >= self->heapThreshold && self->worstEvalue <= self->ecutoff;
}


/** Initialize a new SWheap; parameters to this function correspond
 * directly to fields in the SWheap 
 * @param self the object to be filled [in|out]
 * @param capacity size of heap [in]
 * @param heapThreshold  items always inserted if fewer than this number in heap [in]
 * @param ecutoff items with a expect value less than this will always be inserted into heap [in]
 */
static void
SWheapInitialize(SWheap * self,
                 Int4 capacity,
                 Int4 heapThreshold,
                 double ecutoff)
{
  self->n             = 0;
  self->heapThreshold = heapThreshold;
  self->ecutoff       = ecutoff;
  self->heapArray     = NULL;
  self->capacity      = 0;
  self->worstEvalue   = 0;
  /* Begin life as a list */
  self->array =
    (SWheapRecord *) calloc(1, (capacity + 1) * sizeof(SWheapRecord));
  self->capacity      = capacity;
}


/** Release the storage associated with the fields of a SWheap. Don't
 * delete the SWheap structure itself.
 * @param self record to be cleared [in][out]
 */
static void
SWheapRelease(SWheap * self)
{
  if(self->heapArray) free(self->heapArray);
  if(self->array) free(self->array);

  self->n = self->capacity = self->heapThreshold = 0;
  self->heapArray = NULL;
}


/** Remove and return the element in the SWheap with largest (worst) evalue
 * @param self heap that contains record to be removed [in]
 * @return record that was removed
 */
static SWResults *
SWheapPop(SWheap * self)
{
  SWResults *results = NULL;

  ConvertToHeap(self);
  if(self->n > 0) { /* The heap is not empty */
    SWheapRecord *first, *last; /* The first and last elements of the
                                 * array that represents the heap. */
    first = &self->heapArray[1];
    last  = &self->heapArray[self->n];

    results = first->theseAlignments;
    
    first->theseAlignments = last->theseAlignments;
    first->bestEvalue      = last->bestEvalue;

    SWheapifyDown(self->heapArray, 1, --self->n);
  }
  
  KAPPA_ASSERT(SWheapIsValid(self->heapArray, 1, self->n));

  return results;
}


/** Convert a SWheap to a flat list of SWResults. Note that there
 * may be more than one alignment per match.  The list of all
 * alignments are sorted by the following keys:
 * - First by the evalue the best alignment between the query and a
 *   particular matching sequence;
 * - Second by the subject_index of the matching sequence; and
 * - Third by the evalue of each individual alignment.
 * @param self heap to be "flattened" [in]
 * @return "flattened" version of the input 
 */
static SWResults *
SWheapToFlatList(SWheap * self)
{
  SWResults *list = NULL;       /* the new list of SWResults */
  SWResults *result;            /* the next list of alignments to be
                                   prepended to "list" */

  while(NULL != (result = SWheapPop(self))) {
    SWResults *head, *remaining;     /* The head and remaining
                                        elements in a list of
                                        alignments to be prepended to
                                        "list" */
    remaining = result;
    while(NULL != (head = remaining)) {
      remaining   = head->next;
      head->next  = list;
      list        = head;
    }
  }

  return list;
}

/** keeps one row of the Smith-Waterman matrix
 */
typedef struct SWpairs {
  Int4 noGap;
  Int4 gapExists;
} SWpairs;


/** computes Smith-Waterman local alignment score and returns the
 * evalue
 *
 * @param matchSeq is a database sequence matched by this query [in]
 * @param matchSeqLength is the length of matchSeq in amino acids [in]
 * @param query is the input query sequence [in]
 * @param queryLength is the length of query [in]
 * @param matrix is the position-specific matrix associated with query [in]
 * @param gapOpen is the cost of opening a gap [in]
 * @param gapExtend is the cost of extending an existing gap by 1 position [in]
 * @param matchSeqEnd returns the final position in the matchSeq of an optimal
 *  local alignment [in]
 * @param queryEnd returns the final position in query of an optimal
 *  local alignment [in]
 * matchSeqEnd and queryEnd can be used to run the local alignment in reverse
 *  to find optimal starting positions [in]
 * @param score is used to pass back the optimal score [in]
 * @param kbp holds the Karlin-Altschul parameters [in]
 * @param effSearchSpace effective search space for calculation of expect value [in]
 * @param positionSpecific determines whether matrix is position specific or not [in]
 * @return the expect value of the alignment
*/

static double BLbasicSmithWatermanScoreOnly(Uint1 * matchSeq, 
   Int4 matchSeqLength, Uint1 *query, Int4 queryLength, Int4 **matrix, 
   Int4 gapOpen, Int4 gapExtend,  Int4 *matchSeqEnd, Int4 *queryEnd, Int4 *score,
   Blast_KarlinBlk* kbp, Int8 effSearchSpace, Boolean positionSpecific)
{

   Int4 bestScore; /*best score seen so far*/
   Int4 newScore;  /* score of next entry*/
   Int4 bestMatchSeqPos, bestQueryPos; /*position ending best score in
                           matchSeq and query sequences*/
   SWpairs *scoreVector; /*keeps one row of the Smith-Waterman matrix
                           overwrite old row with new row*/
   Int4 *matrixRow; /*one row of score matrix*/
   Int4 newGapCost; /*cost to have a gap of one character*/
   Int4 prevScoreNoGapMatchSeq; /*score one row and column up
                               with no gaps*/
   Int4 prevScoreGapMatchSeq;   /*score if a gap already started in matchSeq*/
   Int4 continueGapScore; /*score for continuing a gap in matchSeq*/
   Int4 matchSeqPos, queryPos; /*positions in matchSeq and query*/
   double returnEvalue; /*e-value to return*/


   scoreVector = (SWpairs *) calloc(1, matchSeqLength * sizeof(SWpairs));
   bestMatchSeqPos = 0;
   bestQueryPos = 0;
   bestScore = 0;
   newGapCost = gapOpen + gapExtend;
   for (matchSeqPos = 0; matchSeqPos < matchSeqLength; matchSeqPos++) {
     scoreVector[matchSeqPos].noGap = 0;
     scoreVector[matchSeqPos].gapExists = -(gapOpen);
   }
   for(queryPos = 0; queryPos < queryLength; queryPos++) {  
     if (positionSpecific)
       matrixRow = matrix[queryPos];
     else
       matrixRow = matrix[query[queryPos]];
     newScore = 0;
     prevScoreNoGapMatchSeq = 0;
     prevScoreGapMatchSeq = -(gapOpen);
     for(matchSeqPos = 0; matchSeqPos < matchSeqLength; matchSeqPos++) {
       /*testing scores with a gap in matchSeq, either starting a new
         gap or extending an existing gap*/
       if ((newScore = newScore - newGapCost) > 
	   (prevScoreGapMatchSeq = prevScoreGapMatchSeq - gapExtend))
         prevScoreGapMatchSeq = newScore;
       /*testing scores with a gap in query, either starting a new
         gap or extending an existing gap*/
       if ((newScore = scoreVector[matchSeqPos].noGap - newGapCost) >
           (continueGapScore = scoreVector[matchSeqPos].gapExists - gapExtend))
         continueGapScore = newScore;
       /*compute new score extending one position in matchSeq and query*/
       newScore = prevScoreNoGapMatchSeq + matrixRow[matchSeq[matchSeqPos]];
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
   sfree(scoreVector);
   if (bestScore < 0)
     bestScore = 0;
   *matchSeqEnd = bestMatchSeqPos;
   *queryEnd = bestQueryPos;
   *score = bestScore;
   returnEvalue = BLAST_KarlinStoE_simple(bestScore,kbp, effSearchSpace);
   return(returnEvalue);
}

/** computes where optimal Smith-Waterman local alignment starts given the
 *  ending positions and score
 *  matchSeqEnd and queryEnd can be used to run the local alignment in reverse
 *  to find optimal starting positions
 *  these are passed back in matchSeqStart and queryStart
 *  the optimal score is passed in to check when it has
 *  been reached going backwards
 * the score is also returned
 * @param matchSeq is a database sequence matched by this query [in]
 * @param matchSeqLength is the length of matchSeq in amino acids [in]
 * @param query is the input query sequence  [in]
 * @param matrix is the position-specific matrix associated with query
 *      or the standard matrix [in]
 * @param gapOpen is the cost of opening a gap [in]
 * @param gapExtend is the cost of extending an existing gap by 1 position [in]
 * @param matchSeqEnd is the final position in the matchSeq of an optimal
 *      local alignment [in]
 * @param queryEnd is the final position in query of an optimal
 *  local alignment [in]
 * @param score optimal score to be obtained [in]
 * @param matchSeqStart starting point of optimal alignment [out]
 * @param queryStart starting point of optimal alignment [out]
 * @param positionSpecific determines whether matrix is position specific or not
*/
  
static Int4 BLSmithWatermanFindStart(Uint1 * matchSeq, 
   Int4 matchSeqLength, Uint1 *query, Int4 **matrix, 
   Int4 gapOpen, Int4 gapExtend,  Int4 matchSeqEnd, Int4 queryEnd, Int4 score,
   Int4 *matchSeqStart, Int4 *queryStart, Boolean positionSpecific)
{

   Int4 bestScore; /*best score seen so far*/
   Int4 newScore;  /* score of next entry*/
   Int4 bestMatchSeqPos, bestQueryPos; /*position starting best score in
                           matchSeq and database sequences*/
   SWpairs *scoreVector; /*keeps one row of the Smith-Waterman matrix
                           overwrite old row with new row*/
   Int4 *matrixRow; /*one row of score matrix*/
   Int4 newGapCost; /*cost to have a gap of one character*/
   Int4 prevScoreNoGapMatchSeq; /*score one row and column up
                               with no gaps*/
   Int4 prevScoreGapMatchSeq;   /*score if a gap already started in matchSeq*/
   Int4 continueGapScore; /*score for continuing a gap in query*/
   Int4 matchSeqPos, queryPos; /*positions in matchSeq and query*/

   scoreVector = (SWpairs *) calloc(1, matchSeqLength * sizeof(SWpairs));
   bestMatchSeqPos = 0;
   bestQueryPos = 0;
   bestScore = 0;
   newGapCost = gapOpen + gapExtend;
   for (matchSeqPos = 0; matchSeqPos < matchSeqLength; matchSeqPos++) {
     scoreVector[matchSeqPos].noGap = 0;
     scoreVector[matchSeqPos].gapExists = -(gapOpen);
   }
   for(queryPos = queryEnd; queryPos >= 0; queryPos--) {  
     if (positionSpecific)
       matrixRow = matrix[queryPos];
     else
       matrixRow = matrix[query[queryPos]];
     newScore = 0;
     prevScoreNoGapMatchSeq = 0;
     prevScoreGapMatchSeq = -(gapOpen);
     for(matchSeqPos = matchSeqEnd; matchSeqPos >= 0; matchSeqPos--) {
       /*testing scores with a gap in matchSeq, either starting a new
         gap or extending an existing gap*/
       if ((newScore = newScore - newGapCost) > 
	   (prevScoreGapMatchSeq = prevScoreGapMatchSeq - gapExtend))
         prevScoreGapMatchSeq = newScore;
       /*testing scores with a gap in query, either starting a new
         gap or extending an existing gap*/
       if ((newScore = scoreVector[matchSeqPos].noGap - newGapCost) >
           (continueGapScore = scoreVector[matchSeqPos].gapExists - gapExtend))
         continueGapScore = newScore;
       /*compute new score extending one position in matchSeq and query*/
       newScore = prevScoreNoGapMatchSeq + matrixRow[matchSeq[matchSeqPos]];
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
       if (bestScore >= score)
         break;
     }
     if (bestScore >= score)
       break;
   }
   sfree(scoreVector);
   if (bestScore < 0)
     bestScore = 0;
   *matchSeqStart = bestMatchSeqPos;
   *queryStart = bestQueryPos;
   return(bestScore);
}


/** computes Smith-Waterman local alignment score and returns the
 *  evalue assuming some positions are forbidden
 *  matchSeqEnd and query can be used to run the local alignment in reverse
 *  to find optimal starting positions
 * @param matchSeq is the matchSeq sequence [in]
 * @param matchSeqLength is the length of matchSeq in amino acids [in]
 * @param query is the input query sequence  [in]
 * @param queryLength is the length of query [in]
 * @param matrix is either the position-specific matrix associated with query
 *     or the standard matrix [in]
 * @param gapOpen is the cost of opening a gap [in]
 * @param gapExtend is the cost of extending an existing gap by 1 position [in]
 * @param matchSeqEnd returns the final position in the matchSeq of an optimal
 *     local alignment [in]
 * @param queryEnd returns the final position in query of an optimal
 *     local alignment [in]
 * @param score is used to pass back the optimal score [out]
 * @param kbp holds the Karlin-Altschul parameters  [in]
 * @param effSearchSpace effective search space [in]
 * @param numForbidden number of forbidden ranges [in]
 * @param forbiddenRanges lists areas that should not be aligned [in]
 * @param positionSpecific determines whether matrix is position specific or not [in]
*/


static double BLspecialSmithWatermanScoreOnly(Uint1 * matchSeq, 
   Int4 matchSeqLength, Uint1 *query, Int4 queryLength, Int4 **matrix, 
   Int4 gapOpen, Int4 gapExtend,  Int4 *matchSeqEnd, Int4 *queryEnd, Int4 *score,
   Blast_KarlinBlk* kbp,  Int8 effSearchSpace,
   Int4 *numForbidden, Int4 ** forbiddenRanges, Boolean positionSpecific)
{

   Int4 bestScore; /*best score seen so far*/
   Int4 newScore;  /* score of next entry*/
   Int4 bestMatchSeqPos, bestQueryPos; /*position ending best score in
                           matchSeq and database sequences*/
   SWpairs *scoreVector; /*keeps one row of the Smith-Waterman matrix
                           overwrite old row with new row*/
   Int4 *matrixRow; /*one row of score matrix*/
   Int4 newGapCost; /*cost to have a gap of one character*/
   Int4 prevScoreNoGapMatchSeq; /*score one row and column up
                               with no gaps*/
   Int4 prevScoreGapMatchSeq;   /*score if a gap already started in matchSeq*/
   Int4 continueGapScore; /*score for continuing a gap in query*/
   Int4 matchSeqPos, queryPos; /*positions in matchSeq and query*/
   double returnEvalue; /*e-value to return*/
   Boolean forbidden; /*is this position forbidden?*/
   Int4 f; /*index over forbidden positions*/


   scoreVector = (SWpairs *) calloc(1, matchSeqLength * sizeof(SWpairs));
   bestMatchSeqPos = 0;
   bestQueryPos = 0;
   bestScore = 0;
   newGapCost = gapOpen + gapExtend;
   for (matchSeqPos = 0; matchSeqPos < matchSeqLength; matchSeqPos++) {
     scoreVector[matchSeqPos].noGap = 0;
     scoreVector[matchSeqPos].gapExists = -(gapOpen);
   }
   for(queryPos = 0; queryPos < queryLength; queryPos++) {  
     if (positionSpecific)
       matrixRow = matrix[queryPos];
     else
       matrixRow = matrix[query[queryPos]];
     newScore = 0;
     prevScoreNoGapMatchSeq = 0;
     prevScoreGapMatchSeq = -(gapOpen);
     for(matchSeqPos = 0; matchSeqPos < matchSeqLength; matchSeqPos++) {
       /*testing scores with a gap in matchSeq, either starting a new
         gap or extending an existing gap*/
       if ((newScore = newScore - newGapCost) > 
	   (prevScoreGapMatchSeq = prevScoreGapMatchSeq - gapExtend))
         prevScoreGapMatchSeq = newScore;
       /*testing scores with a gap in query, either starting a new
         gap or extending an existing gap*/
       if ((newScore = scoreVector[matchSeqPos].noGap - newGapCost) >
           (continueGapScore = scoreVector[matchSeqPos].gapExists - gapExtend))
         continueGapScore = newScore;
       /*compute new score extending one position in matchSeq and query*/
       forbidden = FALSE;
       for(f = 0; f < numForbidden[queryPos]; f++) {
         if ((matchSeqPos >= forbiddenRanges[queryPos][2 * f]) &&
	     (matchSeqPos <= forbiddenRanges[queryPos][2*f + 1])) {
	   forbidden = TRUE;
	   break;
	 }
       }
       if (forbidden)
         newScore = BLAST_SCORE_MIN;
       else
	 newScore = prevScoreNoGapMatchSeq + matrixRow[matchSeq[matchSeqPos]];
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
   sfree(scoreVector);
   if (bestScore < 0)
     bestScore = 0;
   *matchSeqEnd = bestMatchSeqPos;
   *queryEnd = bestQueryPos;
   *score = bestScore;
   returnEvalue = BLAST_KarlinStoE_simple(bestScore,kbp, effSearchSpace);
   return(returnEvalue);
}

/** computes where optimal Smith-Waterman local alignment starts given the
 *  ending positions.   matchSeqEnd and queryEnd can be used to run the local alignment in reverse
 *  to find optimal starting positions
 * these are passed back in matchSeqStart and queryStart
 * the optimal score is passed in to check when it has
 *  been reached going backwards the score is also returned
 * @param matchSeq is the matchSeq sequence [in]
 * @param matchSeqLength is the length of matchSeq in amino acids [in]
 * @param query is the sequence corresponding to some matrix profile [in]
 * @param matrix is the position-specific matrix associated with query [in]
 * @param gapOpen is the cost of opening a gap [in]
 * @param gapExtend is the cost of extending an existing gap by 1 position [in]
 * @param matchSeqEnd is the final position in the matchSeq of an optimal
 *  local alignment [in]
 * @param queryEnd is the final position in query of an optimal
 *  local alignment [in]
 * @param score optimal score is passed in to check when it has
 *  been reached going backwards [in]
 * @param matchSeqStart optimal starting point [in]
 * @param queryStart optimal starting point [in]
 * @param numForbidden array of regions not to be aligned. [in]
 * @param numForbidden array of regions not to be aligned. [in]
 * @param forbiddenRanges regions not to be aligned. [in]
 * @param positionSpecific determines whether matrix is position specific or not
 * @return the score found
*/

static Int4 BLspecialSmithWatermanFindStart(Uint1 * matchSeq, 
   Int4 matchSeqLength, Uint1 *query, Int4 **matrix, 
   Int4 gapOpen, Int4 gapExtend,  Int4 matchSeqEnd, Int4 queryEnd, Int4 score,
   Int4 *matchSeqStart, Int4 *queryStart, Int4 *numForbidden, 
   Int4 ** forbiddenRanges, Boolean positionSpecific)
{

   Int4 bestScore; /*best score seen so far*/
   Int4 newScore;  /* score of next entry*/
   Int4 bestMatchSeqPos, bestQueryPos; /*position starting best score in
                           matchSeq and database sequences*/
   SWpairs *scoreVector; /*keeps one row of the Smith-Waterman matrix
                           overwrite old row with new row*/
   Int4 *matrixRow; /*one row of score matrix*/
   Int4 newGapCost; /*cost to have a gap of one character*/
   Int4 prevScoreNoGapMatchSeq; /*score one row and column up
                               with no gaps*/
   Int4 prevScoreGapMatchSeq;   /*score if a gap already started in matchSeq*/
   Int4 continueGapScore; /*score for continuing a gap in query*/
   Int4 matchSeqPos, queryPos; /*positions in matchSeq and query*/
   Boolean forbidden; /*is this position forbidden?*/
   Int4 f; /*index over forbidden positions*/

   scoreVector = (SWpairs *) calloc(1, matchSeqLength * sizeof(SWpairs));
   bestMatchSeqPos = 0;
   bestQueryPos = 0;
   bestScore = 0;
   newGapCost = gapOpen + gapExtend;
   for (matchSeqPos = 0; matchSeqPos < matchSeqLength; matchSeqPos++) {
     scoreVector[matchSeqPos].noGap = 0;
     scoreVector[matchSeqPos].gapExists = -(gapOpen);
   }
   for(queryPos = queryEnd; queryPos >= 0; queryPos--) {  
     if (positionSpecific)
       matrixRow = matrix[queryPos];
     else
       matrixRow = matrix[query[queryPos]];
     newScore = 0;
     prevScoreNoGapMatchSeq = 0;
     prevScoreGapMatchSeq = -(gapOpen);
     for(matchSeqPos = matchSeqEnd; matchSeqPos >= 0; matchSeqPos--) {
       /*testing scores with a gap in matchSeq, either starting a new
         gap or extending an existing gap*/
       if ((newScore = newScore - newGapCost) > 
	   (prevScoreGapMatchSeq = prevScoreGapMatchSeq - gapExtend))
         prevScoreGapMatchSeq = newScore;
       /*testing scores with a gap in query, either starting a new
         gap or extending an existing gap*/
       if ((newScore = scoreVector[matchSeqPos].noGap - newGapCost) >
           (continueGapScore = scoreVector[matchSeqPos].gapExists - gapExtend))
         continueGapScore = newScore;
       /*compute new score extending one position in matchSeq and query*/
       forbidden = FALSE;
       for(f = 0; f < numForbidden[queryPos]; f++) {
         if ((matchSeqPos >= forbiddenRanges[queryPos][2 * f]) &&
	     (matchSeqPos <= forbiddenRanges[queryPos][2*f + 1])) {
	   forbidden = TRUE;
	   break;
	 }
       }
       if (forbidden)
         newScore = BLAST_SCORE_MIN;
       else
	 newScore = prevScoreNoGapMatchSeq + matrixRow[matchSeq[matchSeqPos]];
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
       if (bestScore >= score)
         break;
     }
     if (bestScore >= score)
       break;
   }
   sfree(scoreVector);
   if (bestScore < 0)
     bestScore = 0;
   *matchSeqStart = bestMatchSeqPos;
   *queryStart = bestQueryPos;
   return(bestScore);
}


/** converts the list of Smith-Waterman alignments to a corresponding list
 * of HSP's. kbp stores parameters for computing the score
 * Code is adapted from procedure output_hits of pseed3.c
 * @param SWAligns List of Smith-Waterman alignments [in]
 * @param hitlist BlastHitList that is filled in [in|out]
 */
static Int2 newConvertSWalignsUpdateHitList(SWResults * SWAligns, BlastHitList* hitList)
{
    BlastHSPList* hspList=NULL;
    SWResults* curSW;
    
    if (SWAligns == NULL)
       return 0;

    curSW = SWAligns;
    while (curSW != NULL) {
        if (hspList == NULL)
        {
             hspList = Blast_HSPListNew(0); 
             hspList->oid = curSW->subject_index;
        }

        Blast_HSPListSaveHSP(hspList, curSW->hsp);
        curSW->hsp = NULL; /* Saved on the hitlist, will be deleted there. */

        /* Changing OID being worked on. */
        if (curSW->next == NULL ||
              curSW->subject_index != curSW->next->subject_index)
        {
             Blast_HitListUpdate(hitList, hspList);
             hspList = NULL;
        }

        curSW = curSW->next;
    }

    return 0;
}


/** allocates  a score matrix with numPositions positions and initializes some 
 * positions on the side
 * @param numPositions length of matrix (or query) [in]
 * @return matrix (Int4**)  
 */
static Int4 **allocateScaledMatrix(Int4 numPositions)
{
  Int4 **returnMatrix; /*allocated matrix to return*/
  Int4 c; /*loop index over characters*/

  returnMatrix = (Int4**) _PSIAllocateMatrix(numPositions+1, BLASTAA_SIZE, sizeof(Int4));
  for(c = 0; c < BLASTAA_SIZE; c++)
    returnMatrix[numPositions][c] = BLAST_SCORE_MIN;
  return(returnMatrix);
}

/** allocate a frequency ratio matrix with numPositions positions and initialize
 *  some positions.
 * @param numPositions the length of matrix or query [in]
 * @return frequency matrix (double**)
 */
static double **allocateStartFreqs(Int4 numPositions)
{
  double **returnMatrix; /*allocated matrix to return*/
  Int4 c; /*loop index over characters*/

  returnMatrix = (double**) _PSIAllocateMatrix(numPositions+1, BLASTAA_SIZE, sizeof(double));
  for(c = 0; c < BLASTAA_SIZE; c++)
    returnMatrix[numPositions][c] = BLAST_SCORE_MIN;
  return(returnMatrix);
}

#if 0 
FIXME delte if not needed
/*deallocate a frequency ratio matrix*/
static void freeStartFreqs(double **matrix, Int4 numPositions)
{
  int row; /*loop index*/

  for(row = 0; row <= numPositions; row++)
    sfree(matrix[row]);
  sfree(matrix);
}
#endif

/*matrix is a position-specific score matrix with matrixLength positions
  queryProbArray is an array containing the probability of occurrence
  of each residue in the query
  scoreArray is an array of probabilities for each score that is
    to be used as a field in return_sfp
  return_sfp is a the structure to be filled in and returned
  range is the size of scoreArray and is an upper bound on the
   difference between maximum score and minimum score in the matrix
  the routine posfillSfp computes the probability of each score weighted
   by the probability of each query residue and fills those probabilities
   into scoreArray and puts scoreArray as a field in
   that in the structure that is returned
   for indexing convenience the field storing scoreArray points to the
   entry for score 0, so that referring to the -k index corresponds to
   score -k */
static Blast_ScoreFreq* notposfillSfp(Int4 **matrix, double *subjectProbArray,  double *queryProbArray, double *scoreArray,  Blast_ScoreFreq* return_sfp, Int4 range)
{
  Int4 minScore, maxScore; /*observed minimum and maximum scores*/
  Int4 i,j,k; /* indices */

  minScore = maxScore = 0;

  for(i = 0; i < BLASTAA_SIZE; i++) {
    for(j = 0 ; j < PRO_TRUE_ALPHABET_SIZE; j++) {
      k = trueCharPositions[j];
      if ((matrix[i][k] != BLAST_SCORE_MIN) && (matrix[i][k] < minScore))
	minScore = matrix[i][k];
      if (matrix[i][k] > maxScore)
        maxScore = matrix[i][k];
    }
  }
  return_sfp->obs_min = minScore;
  return_sfp->obs_max = maxScore;
  for (i = 0; i < range; i++)
    scoreArray[i] = 0.0;
  return_sfp->sprob = &(scoreArray[-minScore]); /*center around 0*/
  for(i = 0; i < BLASTAA_SIZE; i++) {
    for (j = 0; j < PRO_TRUE_ALPHABET_SIZE; j++) {
      k = trueCharPositions[j];
      if(matrix[i][k] >= minScore) {
        return_sfp->sprob[matrix[i][k]] += (queryProbArray[i] * subjectProbArray[k]);
      }
    }
  }
  return_sfp->score_avg = 0;
  for(i = minScore; i <= maxScore; i++)
    return_sfp->score_avg += i * return_sfp->sprob[i];
  return(return_sfp);
}

/*matrix is a position-specific score matrix with matrixLength positions
  subjectProbArray is an array containing the probability of occurrence
  of each residue in the matching sequence often called the subject
  scoreArray is an array of probabilities for each score that is
    to be used as a field in return_sfp
  return_sfp is a the structure to be filled in and returned
  range is the size of scoreArray and is an upper bound on the
   difference between maximum score and minimum score in the matrix
  the routine posfillSfp computes the probability of each score weighted
   by the probability of each query residue and fills those probabilities
   into scoreArray and puts scoreArray as a field in
   that in the structure that is returned
   for indexing convenience the field storing scoreArray points to the
   entry for score 0, so that referring to the -k index corresponds to
   score -k */
static Blast_ScoreFreq* posfillSfp(Int4 **matrix, Int4 matrixLength, double *subjectProbArray, double *scoreArray,  Blast_ScoreFreq* return_sfp, Int4 range)
{
  Int4 minScore, maxScore; /*observed minimum and maximum scores*/
  Int4 i,j,k; /* indices */
  double onePosFrac; /*1/matrix length as a double*/

  minScore = maxScore = 0;

  for(i = 0; i < matrixLength; i++) {
    for(j = 0 ; j < PRO_TRUE_ALPHABET_SIZE; j++) {
      k = trueCharPositions[j];
      if ((matrix[i][k] != BLAST_SCORE_MIN) && (matrix[i][k] < minScore))
	minScore = matrix[i][k];
      if (matrix[i][k] > maxScore)
        maxScore = matrix[i][k];
    }
  }
  return_sfp->obs_min = minScore;
  return_sfp->obs_max = maxScore;
  for (i = 0; i < range; i++)
    scoreArray[i] = 0.0;
  return_sfp->sprob = &(scoreArray[-minScore]); /*center around 0*/
  onePosFrac = 1.0/ ((double) matrixLength);
  for(i = 0; i < matrixLength; i++) {
    for (j = 0; j < PRO_TRUE_ALPHABET_SIZE; j++) {
      k = trueCharPositions[j];
      if(matrix[i][k] >= minScore) {
        return_sfp->sprob[matrix[i][k]] += (onePosFrac * subjectProbArray[k]);
      }
    }
  }
  return_sfp->score_avg = 0;
  for(i = minScore; i <= maxScore; i++)
    return_sfp->score_avg += i * return_sfp->sprob[i];
  return(return_sfp);
}



/*Given a sequence of 'length' amino acid residues, compute the
  probability of each residue and put that in the array resProb*/
static void fillResidueProbability(Uint1* sequence, Int4 length, double * resProb)
{
  Int4 frequency[BLASTAA_SIZE]; /*frequency of each letter*/
  Int4 i; /*index*/
  Int4 localLength; /*reduce for X characters*/

  localLength = length;
  for(i = 0; i < BLASTAA_SIZE; i++)
    frequency[i] = 0;
  for(i = 0; i < length; i++) {
    if (XCHAR != sequence[i])
      frequency[sequence[i]]++;
    else
      localLength--;
  }
  for(i = 0; i < BLASTAA_SIZE; i++) {
    if (frequency[i] == 0)
      resProb[i] = 0.0;
    else
      resProb[i] = ((double) (frequency[i])) /((double) localLength);
  }
}


#define posEpsilon 0.0001

/** Return the a matrix of the frequency ratios that underlie the
 * score matrix being used on this pass. The returned matrix
 * is position-specific, so if we are in the first pass, use
 * query to convert the 20x20 standard matrix into a position-specific
 * variant. matrixName is the name of the underlying 20x20
 * score matrix used. numPositions is the length of the query;
 * startNumerator is the matrix of frequency ratios as stored
 * in posit.h. It needs to be divided by the frequency of the
 * second character to get the intended ratio 
 * @param sbp statistical information for blast [in]
 * @param query the query sequence [in]
 * @param matrixName name of the underlying matrix [in]
 * @param startNumerator matrix of frequency ratios as stored
 *      in posit.h. It needs to be divided by the frequency of the
 *      second character to get the intended ratio [in]
 * @param numPositions length of the query [in]
 */
static double **getStartFreqRatios(BlastScoreBlk* sbp,
					Uint1* query,
					const char *matrixName, 
					double **startNumerator,
					Int4 numPositions) 
{
   Blast_ResFreq* stdrfp; /* gets standard frequencies in prob field */
   double** returnRatios; /*frequency ratios to start investigating each pair*/
   double *standardProb; /*probabilities of each letter*/
   Int4 i,j;  /* Loop indices. */
   SFreqRatios* freqRatios=NULL; /* frequency ratio container for given matrix */

   returnRatios = allocateStartFreqs(numPositions);

   freqRatios = _PSIMatrixFrequencyRatiosNew(matrixName);
   ASSERT(freqRatios);
   if (freqRatios == NULL)
	return NULL;

   for(i = 0; i < numPositions; i++) {
     for(j = 0; j < BLASTAA_SIZE; j++) {
	   returnRatios[i][j] = freqRatios->data[query[i]][j];
     }
   }

   freqRatios = _PSIMatrixFrequencyRatiosFree(freqRatios);

/* FIXME use blast_psi_priv.c:_PSIGetStandardProbabilities when available here. */
   stdrfp = Blast_ResFreqNew(sbp);
   Blast_ResFreqStdComp(sbp,stdrfp); 
   standardProb = calloc(1, BLASTAA_SIZE * sizeof(double));
   for(i = 0; i < BLASTAA_SIZE; i++)
       standardProb[i] = stdrfp->prob[i];
     /*reverse multiplication done in posit.c*/
   for(i = 0; i < numPositions; i++) 
     for(j = 0; j < BLASTAA_SIZE; j++) 
       if ((standardProb[query[i]] > posEpsilon) && (standardProb[j] > posEpsilon) &&     
	     (j != STARCHAR) && (j != XCHAR)
	     && (startNumerator[i][j] > posEpsilon))
	   returnRatios[i][j] = startNumerator[i][j]/standardProb[j];
   stdrfp = Blast_ResFreqDestruct(stdrfp);
   sfree(standardProb);

   return(returnRatios);
}

/** take every entry of startFreqRatios that is not corresponding to
 * a score of BLAST_SCORE_MIN and take its log, divide by Lambda and
 * multiply  by LambdaRatio then round to the nearest integer and
 * put the result in the corresponding entry of matrix.
 * startMatrix and matrix have dimensions numPositions X BLASTAA_SIZE
 * @param matrix preallocated matrix to be filled in [out]
 * @param startMatrix matrix to be scaled up [in]
 * @param startFreqRatios frequency ratios of starting matrix [in]
 * @param numPositions length of query [in]
 * @param Lambda A Karlin-Altschul parameter. [in]
 * @param LambdaRatio ratio of correct Lambda to it's original value [in]
*/
static void scaleMatrix(Int4 **matrix, Int4 **startMatrix, 
			double **startFreqRatios, Int4 numPositions, 
			double Lambda, double LambdaRatio)
{
   Int4 p, c; /*indices over positions and characters*/
   double temp; /*intermediate term in computation*/

   for (p = 0; p < numPositions; p++) {
     for (c = 0; c < BLASTAA_SIZE; c++) {
       if (matrix[p][c] == BLAST_SCORE_MIN)
	 matrix[p][c] = startMatrix[p][c];
       else {
         temp = log(startFreqRatios[p][c]);
         temp = temp/Lambda;
	 temp = temp * LambdaRatio; 
	 matrix[p][c] = BLAST_Nint(temp);
       }
     }
   }
}

/*SCALING_FACTOR is a multiplicative factor used to get more bits of
 * precision in the integer matrix scores. It cannot be arbitrarily
 * large because we do not want total alignment scores to exceedto
 * -(BLAST_SCORE_MIN) */
#define SCALING_FACTOR 32
/** Compute a scaled up version of the standard matrix encoded by matrix name. 
 * Standard matrices are in half-bit units.
 * @param matrix preallocated matrix [in][out]
 * @param matrixName name of matrix (e.g., BLOSUM62, PAM30). [in]
 * @param Lambda A Karlin-Altschul parameter. [in]
*/
static void  computeScaledStandardMatrix(Int4 **matrix, char *matrixName, double Lambda)
{
   int i,j; /*loop indices*/
   double temp; /*intermediate term in computation*/
   SFreqRatios* freqRatios=NULL; /* frequency ratio container for given matrix */

   freqRatios = _PSIMatrixFrequencyRatiosNew(matrixName);
   ASSERT(freqRatios);
   if (freqRatios == NULL)
	return;

   for(i = 0; i < BLASTAA_SIZE; i++)
     for(j = 0; j < BLASTAA_SIZE; j++) {
         if(0.0 == freqRatios->data[i][j])
	   matrix[i][j] = BLAST_SCORE_MIN;
	 else {
	   temp = log(freqRatios->data[i][j])/Lambda;
           matrix[i][j] = BLAST_Nint(temp);
     }
   }

   freqRatios = _PSIMatrixFrequencyRatiosFree(freqRatios);
   return;
}


#if 0 /* FIXME */
/************************************************************
produce a scaled-up version of the position-specific matrix starting from
posFreqs
fillPosMatrix is the matrix to be filled
nonposMatrix is the underlying position-independent matrix, used to
fill positions where frequencies are irrelevant
sbp stores various parameters of the search
*****************************************************************/
void scalePosMatrix(Int4 **fillPosMatrix, Int4 **nonposMatrix, char *matrixName, double **posFreqs, Uint1 *query, Int4 queryLength, BLAST_ScoreBlk* sbp)
{

     posSearchItems *posSearch; /*used to pass data into scaling routines*/
     compactSearchItems *compactSearch; /*used to pass data into scaling routines*/
     Int4 i,j ; /*loop indices*/   
     BLAST_ResFreq* stdrfp; /* gets standard frequencies in prob field */
     Int4 a; /*index over characters*/
     double **standardFreqRatios; /*frequency ratios for standard score matrix*/
     Int4 multiplier; /*bit scale factor for scores*/


     posSearch = (posSearchItems *) calloc (1, sizeof(posSearchItems));
     compactSearch = (compactSearchItems *) calloc (1, sizeof(compactSearchItems));
     posSearch->posMatrix = (Int4 **) calloc((queryLength + 1), sizeof(Int4 *));
     posSearch->posPrivateMatrix = fillPosMatrix;
     posSearch->posFreqs = posFreqs;
     for(i = 0; i <= queryLength; i++) 
       posSearch->posMatrix[i] = (Int4 *) calloc(BLASTAA_SIZE, sizeof(Int4));

     compactSearch->query = (Uint1*) query;
     compactSearch->qlength = queryLength;
     compactSearch->alphabetSize = BLASTAA_SIZE;
     compactSearch->gapped_calculation = TRUE;
     compactSearch->matrix = nonposMatrix;
     compactSearch->lambda =  sbp->kbp_gap_std[0]->Lambda;
     compactSearch->kbp_std = sbp->kbp_std;
     compactSearch->kbp_psi = sbp->kbp_psi;
     compactSearch->kbp_gap_psi = sbp->kbp_gap_psi;
     compactSearch->kbp_gap_std = sbp->kbp_gap_std;
     compactSearch->lambda_ideal = sbp->kbp_ideal->Lambda;
     compactSearch->K_ideal = sbp->kbp_ideal->K;

     stdrfp = BlastResFreqNew(sbp);
     BlastResFreqStdComp(sbp,stdrfp); 
     compactSearch->standardProb = calloc(compactSearch->alphabetSize, sizeof(double));
     for(a = 0; a < compactSearch->alphabetSize; a++)
       compactSearch->standardProb[a] = stdrfp->prob[a];
     stdrfp = BlastResFreqDestruct(stdrfp);

     standardFreqRatios = (double **) calloc(BLASTAA_SIZE, sizeof(double *));
     for (i = 0; i < BLASTAA_SIZE; i++)
       standardFreqRatios[i] = (double *) calloc(BLASTAA_SIZE, sizeof(double));

     if ((0 == strcmp(matrixName,"BLOSUM62")) ||
	 (0 == strcmp(matrixName,"BLOSUM62_20"))) {
       multiplier = 2;
       for(i = 0; i < BLASTAA_SIZE; i++)
	 for(j = 0; j < BLASTAA_SIZE; j++)
	   standardFreqRatios[i][j] = BLOSUM62_FREQRATIOS[i][j];
     }
     if (0 == strcmp(matrixName,"BLOSUM62_20A")) {
       multiplier = 2;
       for(i = 0; i < BLASTAA_SIZE; i++)
	 for(j = 0; j < BLASTAA_SIZE; j++)
	   standardFreqRatios[i][j] = 0.9666 * BLOSUM62_FREQRATIOS[i][j];
     }
     if (0 == strcmp(matrixName,"BLOSUM62_20B")) {
       multiplier = 2;
       for(i = 0; i < BLASTAA_SIZE; i++)
	 for(j = 0; j < BLASTAA_SIZE; j++)
	   standardFreqRatios[i][j] = 0.9344 * BLOSUM62_FREQRATIOS[i][j];
     }
     if (0 == strcmp(matrixName,"BLOSUM45")) {
       multiplier = 3;
       for(i = 0; i < BLASTAA_SIZE; i++)
	 for(j = 0; j < BLASTAA_SIZE; j++)
	   standardFreqRatios[i][j] = BLOSUM45_FREQRATIOS[i][j];
     }
     if (0 == strcmp(matrixName,"BLOSUM80")) {
       multiplier = 2;
       for(i = 0; i < BLASTAA_SIZE; i++)
	 for(j = 0; j < BLASTAA_SIZE; j++)
	   standardFreqRatios[i][j] = BLOSUM80_FREQRATIOS[i][j];
     }
     if (0 == strcmp(matrixName,"BLOSUM50")) {
       multiplier = 2;
       for(i = 0; i < BLASTAA_SIZE; i++)
	 for(j = 0; j < BLASTAA_SIZE; j++)
	   standardFreqRatios[i][j] = BLOSUM50_FREQRATIOS[i][j];
     }
     if (0 == strcmp(matrixName,"BLOSUM90")) {
       multiplier = 2;
       for(i = 0; i < PROTEIN_ALPHABET; i++)
	 for(j = 0; j < PROTEIN_ALPHABET; j++)
	   standardFreqRatios[i][j] = BLOSUM90_FREQRATIOS[i][j];
     }
     if (0 == strcmp(matrixName,"PAM250")) {
       multiplier = 2;
       for(i = 0; i < PROTEIN_ALPHABET; i++)
	 for(j = 0; j < PROTEIN_ALPHABET; j++)
	   standardFreqRatios[i][j] = PAM250_FREQRATIOS[i][j];
     }
     if (0 == strcmp(matrixName,"PAM30")) {
       multiplier = 2;
       for(i = 0; i < PROTEIN_ALPHABET; i++)
	 for(j = 0; j < PROTEIN_ALPHABET; j++)
	   standardFreqRatios[i][j] = PAM30_FREQRATIOS[i][j];
     }
     if (0 == strcmp(matrixName,"PAM70")) {
       multiplier = 2;
       for(i = 0; i < PROTEIN_ALPHABET; i++)
	 for(j = 0; j < PROTEIN_ALPHABET; j++)
	   standardFreqRatios[i][j] = PAM70_FREQRATIOS[i][j];
     }

     posFreqsToMatrix(posSearch,compactSearch, standardFreqRatios, multiplier);
     impalaScaling(posSearch, compactSearch, ((double) SCALING_FACTOR), FALSE);

     for(i = 0; i <= queryLength; i++)
       sfree(posSearch->posMatrix[i]);
     for(i = 0; i < PROTEIN_ALPHABET; i++)
       sfree(standardFreqRatios[i]);

     sfree(standardFreqRatios);
     sfree(compactSearch->standardProb);
     sfree(posSearch->posMatrix);
     sfree(posSearch);
     sfree(compactSearch);
}
#endif

/**
 * A Kappa_MatchingSequence represents a subject sequence to be aligned
 * with the query.  This abstract sequence is used to hide the
 * complexity associated with actually obtaining and releasing the
 * data for a matching sequence, e.g. reading the sequence from a DB
 * or translating it from a nucleotide sequence. 
 */
struct Kappa_MatchingSequence {
  Int4      length;             /**< length of the sequence */
  Uint1*  sequence;           /**< the sequence data */
  Uint1*  filteredSequence;   /**< a copy of the sequence data that has 
                                   been filtered */
  Uint1*  filteredSequenceStart;      /**< the address of the chunk of
                                           memory that has been
                                           allocated to hold
                                           "filterSequence". */
  BLAST_SequenceBlk* seq_blk;   /**< sequence blk for "database" sequence. */
};
typedef struct Kappa_MatchingSequence Kappa_MatchingSequence;


#define BLASTP_MASK_INSTRUCTIONS "S 10 1.8 2.1"

/** Initialize a new matching sequence, obtaining the data from an
 * appropriate location 
 * @param self the Kappa_MatchingSequence to be filled in [in|out]
 * @param seqSrc Used to access match sequences [in]
 * @param subject_id ordinal ID of matching sequence [in]
 */
static void
Kappa_MatchingSequenceInitialize(Kappa_MatchingSequence * self,
                                 const BlastSeqSrc* seqSrc,
                                 Int4 subject_id)
{
  GetSeqArg seq_arg;

  memset((void*) &seq_arg, 0, sizeof(seq_arg));
  seq_arg.oid = subject_id;
  seq_arg.encoding = BLASTP_ENCODING;

  BlastSequenceBlkClean(seq_arg.seq);

  if (BLASTSeqSrcGetSequence(seqSrc, (void*) &seq_arg) < 0)
	return;

  self->length = BLASTSeqSrcGetSeqLen(seqSrc, &seq_arg);

  self->sequence = BlastMemDup(seq_arg.seq->sequence, (1+self->length)*sizeof(Uint1));

  self->filteredSequenceStart = calloc((self->length + 2), sizeof(Uint1));
  self->filteredSequence      = self->filteredSequenceStart + 1;
  memcpy(self->filteredSequence, self->sequence, self->length);

#ifndef KAPPA_NO_SEG_SEQUENCE
/*take as input an amino acid  string and its length; compute a filtered
  amino acid string and return the filtered string*/
  {{
     BlastSeqLoc* mask_seqloc;
     const Uint1 k_program_name = blast_type_blastp;

     BlastSetUp_Filter(k_program_name, self->sequence, self->length,
        0, BLASTP_MASK_INSTRUCTIONS, NULL, &mask_seqloc);

     Blast_MaskTheResidues(self->filteredSequence, self->length, FALSE, mask_seqloc, FALSE, 0);

     mask_seqloc = BlastSeqLocFree(mask_seqloc);
  }}
#endif
  self->seq_blk = NULL;
  BlastSetUp_SeqBlkNew(self->filteredSequence, self->length, 0, &(self->seq_blk), FALSE);
  return;

}


/** Release the data associated with a matching sequence
 * @param self the Kappa_MatchingSequence whose data will be freed [in|out]
 */
static void
Kappa_MatchingSequenceRelease(Kappa_MatchingSequence * self)
{
  if(self->sequence != self->filteredSequence) {
    sfree(self->filteredSequenceStart);
  }
  sfree(self->sequence);
  self->seq_blk = BlastSequenceBlkFree(self->seq_blk);
}


/** An instance of Kappa_ForbiddenRanges is used by the Smith-Waterman
 * algorithm to represent ranges in the database that are not to be
 * aligned.
 */

struct Kappa_ForbiddenRanges { 
  Int4 *numForbidden;           /**< how many forbidden ranges at each db  
                                  position */
  Int4 **ranges;                /**< forbidden ranges for each database
                                  position */
  Int4   queryLength;           /**< length of query. */
};
typedef struct Kappa_ForbiddenRanges Kappa_ForbiddenRanges;


/** Initialize a new, empty Kappa_ForbiddenRanges
 * @param self object to be initialized [in|out]
 * @param queryLength length of the query [in]
 */
static void
Kappa_ForbiddenRangesInitialize(
  Kappa_ForbiddenRanges * self, 
  Int4 queryLength              
) {
  Int4 f;
  self->queryLength  = queryLength;
  self->numForbidden = (Int4 *) calloc(queryLength, sizeof(Int4));
  self->ranges       = (Int4 **) calloc(queryLength, sizeof(Int4 *));

  for(f = 0; f < queryLength; f++) {
    self->numForbidden[f] = 0;
    self->ranges[f]       = (Int4 *) calloc(2, sizeof(Int4));
    self->ranges[f][0]    = 0;
    self->ranges[f][1]    = 0;
  }
}


/** Reset self to be empty 
 * @param self the object to be reset [in|out]
 */
static void
Kappa_ForbiddenRangesClear(Kappa_ForbiddenRanges * self)
{
  Int4 f;
  for(f = 0; f < self->queryLength; f++) {
    self->numForbidden[f] = 0;
  }
}


/** Add some ranges to self 
 * @param self object to be be "pushed" [in|out]
 * @param queryStart start of the alignment in the query sequence [in]
 * @param queryAlignmentExtent length of the alignment in the query sequence [in]
 * @param matchStart start of the alignment in the subject sequence [in]
 * @param matchAlignmentExtent length of the alignment in the subject sequence  [in]
 */
static void
Kappa_ForbiddenRangesPush(
  Kappa_ForbiddenRanges * self,
  Int4 queryStart,      /* start of the alignment in the query sequence */
  Int4 queryAlignmentExtent,   /* length of the alignment in the query sequence */
  Int4 matchStart,      /* start of the alignment in the subject sequence */
  Int4 matchAlignmentExtent)   /* length of the alignment in the subject  sequence */
{
  Int4 f;
  for(f = queryStart; f < (queryStart + queryAlignmentExtent); f++) {
    Int4 last = 2 * self->numForbidden[f];
    if(0 != last) {    /* we must resize the array */
      self->ranges[f] =
        (Int4 *) realloc(self->ranges[f], (last + 2) * sizeof(Int4));
    }
    self->ranges[f][last]     = matchStart;
    self->ranges[f][last + 1] = matchStart + matchAlignmentExtent;

    self->numForbidden[f]++;
  }
}


/** Release the storage associated with the fields of self, but do not 
 * delete self 
 * @param self the object whose storage will be released [in|out]
 */
static void
Kappa_ForbiddenRangesRelease(Kappa_ForbiddenRanges * self)
{
  Int4 f;
  for(f = 0; f < self->queryLength; f++)  sfree(self->ranges[f]);
  
  sfree(self->ranges);       self->ranges       = NULL;
  sfree(self->numForbidden); self->numForbidden = NULL;
}


/** Redo a S-W alignment using an x-drop alignment.  The result will
 * usually be the same as the S-W alignment. The call to ALIGN
 * attempts to force the endpoints of the alignment to match the
 * optimal endpoints determined by the Smith-Waterman algorithm.
 * ALIGN is used, so that if the data structures for storing BLAST
 * alignments are changed, the code will not break 
 *
 * @param query the query sequence [in]
 * @param queryLength length of the query sequence [in]
 * @param queryStart start of the alignment in the query sequence [in]
 * @param queryEnd end of the alignment in the query sequence,
 *                          as computed by the Smith-Waterman algorithm [in]
 * @param match the subject (database) sequence [in]
 * @param matchLength length of the subject sequence [in]
 * @param matchStart start of the alignment in the subject sequence [in]
 * @param matchEnd end of the alignment in the query sequence,
                           as computed by the Smith-Waterman algorithm [in]
 * @param gap_align parameters for a gapped alignment [in]
 * @param scoringParams Settings for gapped alignment.[in]
 * @param score score computed by the Smith-Waterman algorithm [in]
 * @param localScalingFactor the factor by which the
 *                 scoring system has been scaled in order to obtain
 *                 greater precision [in]
 * @param * queryAlignmentExtent length of the alignment in the query sequence,
 *                          as computed by the x-drop algorithm [out]
 * @param * matchAlignmentExtent length of the alignment in the subject sequence,
 *                          as computed by the x-drop algorithm  [out]
 * @param ** reverseAlignScript alignment information (script) returned by 
                            a x-drop alignment algorithm [out]
 * @param * newScore alignment score computed by the x-drop algorithm [out]
 */
static void
Kappa_SWFindFinalEndsUsingXdrop(
  Uint1* query,       /* the query sequence */
  Int4 queryLength,     /* length of the query sequence */
  Int4 queryStart,      /* start of the alignment in the query sequence */
  Int4 queryEnd,        /* end of the alignment in the query sequence,
                           as computed by the Smith-Waterman algorithm */
  Uint1* match,       /* the subject (database) sequence */
  Int4 matchLength,     /* length of the subject sequence */
  Int4 matchStart,      /* start of the alignment in the subject sequence */
  Int4 matchEnd,        /* end of the alignment in the query sequence,
                           as computed by the Smith-Waterman algorithm */
  BlastGapAlignStruct* gap_align,     /* parameters for a gapped alignment */
  const BlastScoringParameters* scoringParams, /* Settings for gapped alignment. */
  Int4 score,           /* score computed by the Smith-Waterman algorithm */
  double localScalingFactor,       /* the factor by which the
                                         * scoring system has been
                                         * scaled in order to obtain
                                         * greater precision */
  Int4 * queryAlignmentExtent, /* length of the alignment in the query sequence,
                           as computed by the x-drop algorithm */
  Int4 * matchAlignmentExtent, /* length of the alignment in the subject sequence,
                           as computed by the x-drop algorithm */
  Int4 ** reverseAlignScript,   /* alignment information (script)
                                 * returned by a x-drop alignment algorithm */
  Int4 * newScore        /* alignment score computed by the
                                   x-drop algorithm */
) {
  Int4 XdropAlignScore;         /* alignment score obtained using X-dropoff
                                 * method rather than Smith-Waterman */
  Int4 doublingCount = 0;       /* number of times X-dropoff had to be
                                 * doubled */
  do {
    Int4 *alignScript;          /* the alignment script that will be
                                   generated below by the ALIGN
                                   routine. */
    
    *reverseAlignScript = alignScript =
      (Int4 *) calloc(matchLength, (queryLength + 3) * sizeof(Int4));

    XdropAlignScore =
      ALIGN_EX(&(query[queryStart]) - 1, &(match[matchStart]) - 1,
            queryEnd - queryStart + 1, matchEnd - matchStart + 1,
            *reverseAlignScript, queryAlignmentExtent, matchAlignmentExtent, &alignScript,
            gap_align, scoringParams, queryStart - 1, FALSE, FALSE);

    gap_align->gap_x_dropoff *= 2;
    doublingCount++;
    if((XdropAlignScore < score) && (doublingCount < 3)) {
      sfree(*reverseAlignScript);
    }
  } while((XdropAlignScore < score) && (doublingCount < 3));

  *newScore = BLAST_Nint(((double) XdropAlignScore) / localScalingFactor);
}


/** A Kappa_SearchParameters represents the data needed by
 * RedoAlignmentCore to adjust the parameters of a search, including
 * the original value of these parameters 
 */
struct Kappa_SearchParameters {
  Int4          gapOpen;        /**< a penalty for the existence of a gap */
  Int4          gapExtend;      /**< a penalty for each residue (or nucleotide) 
                                 * in the gap */
  Int4          gapDecline;     /**< a penalty for declining to align a pair of 
                                 * residues */
  Int4          mRows, nCols;   /**< the number of rows an columns in a scoring 
                                 * matrix */
  double   scaledUngappedLambda;   /**< The value of Karlin-Altchul
                                         * parameter lambda, rescaled
                                         * to allow scores to have
                                         * greater precision */
  Int4 **startMatrix, **origMatrix;
  SFreqRatios* sFreqRatios;        /**< Stores the frequency ratios along 
                                         *  with their bit scale factor */
  double **startFreqRatios;        /**< frequency ratios to start
                                         * investigating each pair */
  double  *scoreArray;      /**< array of score probabilities */
  double  *resProb;         /**< array of probabilities for each residue in 
                                  * a matching sequence */
  double  *queryProb;       /**< array of probabilities for each residue in 
                                  * the query */
  Boolean       adjustParameters;

  Blast_ScoreFreq* return_sfp;        /**< score frequency pointers to
                                         * compute lambda */
  Blast_KarlinBlk *kbp_gap_orig, **orig_kbp_gap_array; /* FIXME, AS only had one * on orig_kbp_gap_array, check with him about this. */
  double scale_factor;      /**< The original scale factor (to be restored). */
};
typedef struct Kappa_SearchParameters Kappa_SearchParameters;


/** Release the date associated with a Kappa_SearchParameters and
 * delete the object 
 * @param searchParams the object to be deleted [in][out]
*/
static void
Kappa_SearchParametersFree(Kappa_SearchParameters ** searchParams)
{
  /* for convenience, remove one level of indirection from searchParams */
  Kappa_SearchParameters *sp = *searchParams; 

  if(sp->kbp_gap_orig) Blast_KarlinBlkDestruct(sp->kbp_gap_orig);

  /* An extra row is added at end during allocation. */
  if(sp->startMatrix)     _PSIDeallocateMatrix((void**) sp->startMatrix, 1+sp->mRows);
  if(sp->origMatrix)      _PSIDeallocateMatrix((void**) sp->origMatrix, 1+sp->mRows);
  if(sp->sFreqRatios)     _PSIMatrixFrequencyRatiosFree(sp->sFreqRatios);
/*
  if(sp->startFreqRatios) freeStartFreqs(sp->startFreqRatios, sp->mRows);
*/

  if(sp->return_sfp) sfree(sp->return_sfp);
  if(sp->scoreArray) sfree(sp->scoreArray);
  if(sp->resProb)    sfree(sp->resProb);
  if(sp->queryProb)  sfree(sp->queryProb);

  sfree(*searchParams);
  *searchParams = NULL;
}


/** Create a new instance of Kappa_SearchParameters 
 * @param number of rows in the scoring matrix [in]
 * @param adjustParameters if true, use composition-based statistics [in]
 * @param positionBased if true, the search is position-based [in]
*/
static Kappa_SearchParameters *
Kappa_SearchParametersNew(
  Int4 rows,                    /* number of rows in the scoring matrix */
  Boolean adjustParameters,     /* if true, use composition-based statistics */
  Boolean positionBased         /* if true, the search is position-based */
) {
  Kappa_SearchParameters *sp;   /* the new object */
  sp = malloc(sizeof(Kappa_SearchParameters));

  sp->orig_kbp_gap_array = NULL;
  
  sp->mRows = positionBased ? rows : BLASTAA_SIZE;
  sp->nCols = BLASTAA_SIZE;
    
  sp->kbp_gap_orig     = NULL;
  sp->startMatrix      = NULL;
  sp->origMatrix       = NULL;
  sp->sFreqRatios      = NULL;
  sp->startFreqRatios  = NULL;
  sp->return_sfp       = NULL;
  sp->scoreArray       = NULL;
  sp->resProb          = NULL;
  sp->queryProb        = NULL;
  sp->adjustParameters = adjustParameters;
  
  if(adjustParameters) {
    sp->kbp_gap_orig = Blast_KarlinBlkCreate();
    sp->startMatrix  = allocateScaledMatrix(sp->mRows);
    sp->origMatrix   = allocateScaledMatrix(sp->mRows);
    
    sp->resProb    =
      (double *) calloc(BLASTAA_SIZE, sizeof(double));
    sp->scoreArray =
      (double *) calloc(scoreRange, sizeof(double));
    sp->return_sfp =
      (Blast_ScoreFreq*) calloc(1, sizeof(Blast_ScoreFreq));
    
    if(!positionBased) {
      sp->queryProb =
        (double *) calloc(BLASTAA_SIZE, sizeof(double));
    }
  }
  /* end if(adjustParameters) */

  return sp;
}


/** Record the initial value of the search parameters that are to be
 * adjusted. 
 * @param searchParams the object to be filled in [in|out]
 * @param queryBlk query sequence [in]
 * @param queryInfo query sequence information [in]
 * @param sbp Scoring Blk (contains Karlin-Altschul parameters) [in]
 * @param scoring gap-open/extend/decline_align information [in]
 */
static void
Kappa_RecordInitialSearch(Kappa_SearchParameters * searchParams, 
                          BLAST_SequenceBlk * queryBlk,
                          BlastQueryInfo* queryInfo,
                          BlastScoreBlk* sbp,
                          const BlastScoringParameters* scoring)
{
  Uint1* query;               /* the query sequence */
  Int4 queryLength;             /* the length of the query sequence */
  const Int4 k_context_offset = queryInfo->context_offsets[0];  /* offset in buffer of start of query. */

  query = &queryBlk->sequence[k_context_offset];
  queryLength = BLAST_GetQueryLength(queryInfo, 0);

  if(searchParams->adjustParameters) {
    Int4 i, j;
    Blast_KarlinBlk* kbp;     /* statistical parameters used to evaluate a
                                 * query-subject pair */
    Int4 **matrix;       /* matrix used to score a local
                                   query-subject alignment */
    Boolean positionBased = FALSE; /* FIXME, how is this set in scoring options? */

    if(positionBased) {
      kbp    = sbp->kbp_gap_psi[0];
      matrix = sbp->posMatrix;
    } else {
      kbp    = sbp->kbp_gap_std[0];
      matrix = sbp->matrix;
      fillResidueProbability(query, queryLength, searchParams->queryProb);
    }
    searchParams->gapOpen    = scoring->gap_open;
    searchParams->gapExtend  = scoring->gap_extend;
    searchParams->gapDecline = scoring->decline_align;
    searchParams->scale_factor   = scoring->scale_factor;

    searchParams->orig_kbp_gap_array   = sbp->kbp_gap;

    searchParams->kbp_gap_orig->Lambda = kbp->Lambda;
    searchParams->kbp_gap_orig->K      = kbp->K;
    searchParams->kbp_gap_orig->logK   = kbp->logK;
    searchParams->kbp_gap_orig->H      = kbp->H;

    for(i = 0; i < searchParams->mRows; i++) {
      for(j = 0; j < BLASTAA_SIZE; j++) {
        searchParams->origMatrix[i][j] = matrix[i][j];
      }
    }
  }
}

/** Rescale the search parameters in the search object and options object to
 * obtain more precision.
 * @param sp record of parameters used and frequencies [in|out]
 * @param queryBlk query sequence [in]
 * @param queryInfo query sequence information [in]
 * @param sbp Scoring Blk (contains Karlin-Altschul parameters) [in]
 * @param scoring gap-open/extend/decline_align information [in]
 * @return scaling-factor to be used.
 */
static double
Kappa_RescaleSearch(Kappa_SearchParameters * sp,
                    BLAST_SequenceBlk* queryBlk,
                    BlastQueryInfo* queryInfo,
                    BlastScoreBlk* sbp,
                    BlastScoringParameters* scoringParams)
{
  double localScalingFactor;       /* the factor by which to
                                         * scale the scoring system in
                                         * order to obtain greater
                                         * precision */

  if(!sp->adjustParameters) {
    localScalingFactor = 1.0;
  } else {
    double initialUngappedLambda;  /* initial value of the
                                         * statistical parameter
                                         * lambda used to evaluate
                                         * ungapped alignments */
    Blast_KarlinBlk* kbp;     /* the statistical parameters used to
                                 * evaluate alignments of a
                                 * query-subject pair */
    Uint1* query;             /* the query sequence */
    Int4 queryLength;           /* the length of the query sequence */
    Boolean positionBased=FALSE; /* FIXME, how is this set with options?? */

    if((0 == strcmp(scoringParams->options->matrix, "BLOSUM62_20"))) {
      localScalingFactor = SCALING_FACTOR / 10;
    } else {
      localScalingFactor = SCALING_FACTOR;
    }

    scoringParams->scale_factor = localScalingFactor;

    scoringParams->gap_open   = BLAST_Nint(sp->gapOpen   * localScalingFactor);
    scoringParams->gap_extend = BLAST_Nint(sp->gapExtend * localScalingFactor);
    if(sp->gapDecline != INT2_MAX) {
      scoringParams->decline_align =
        BLAST_Nint(sp->gapDecline * localScalingFactor);
    }

    query = &queryBlk->sequence[0];
    queryLength = BLAST_GetQueryLength(queryInfo, 0);
    if(positionBased) {
      sp->startFreqRatios =
        getStartFreqRatios(sbp, query, scoringParams->options->matrix,
                           sbp->posFreqs, queryLength);
/* FIXME      scalePosMatrix(sp->startMatrix, sbp->matrix, scoringParams->options->matrix,
                     sbp->posFreqs, query, queryLength, sbp);
*/
      initialUngappedLambda = sbp->kbp_psi[0]->Lambda;
    } else {
/*
      sp->startFreqRatios =
        getStartFreqRatios(sbp, query, scoringParams->options->matrix, NULL,
                           PROTEIN_ALPHABET, FALSE);
*/
      sp->sFreqRatios = _PSIMatrixFrequencyRatiosNew(scoringParams->options->matrix);
      sp->startFreqRatios = sp->sFreqRatios->data;
      initialUngappedLambda = sbp->kbp_ideal->Lambda;
    }
    sp->scaledUngappedLambda = initialUngappedLambda / localScalingFactor;
    if(!positionBased) {
      computeScaledStandardMatrix(sp->startMatrix, scoringParams->options->matrix,
                                  sp->scaledUngappedLambda);
    }
    if(positionBased) {
      kbp = sbp->kbp_gap_psi[0];
    } else {
      kbp = sbp->kbp_gap_std[0];
    }
    kbp->Lambda /= localScalingFactor;
    kbp->logK = log(kbp->K);
  }

  return localScalingFactor;
}


/*LambdaRatioLowerBound is used when the expected score is too large
 * causing impalaKarlinLambdaNR to give a Lambda estimate that
 * is too small, or to fail entirely returning -1*/
#define LambdaRatioLowerBound 0.5

/** Adjust the search parameters
 * @param searchParams a record of the initial search parameters [in|out]
 * @param queryLength length of query sequence [in]
 * @param filteredSequence a filtered subject sequence [in] 
 * @param length length of the filtered sequence  [in]
 * @param matrix a scoring matrix to be adjusted [out]
 * @return scaling-factor to be used.
 */
static Int4
Kappa_AdjustSearch(
  Kappa_SearchParameters * sp,  /* a record of the initial search parameters */
  Int4 queryLength,             /* length of the query. */
  Uint1* filteredSequence,    /* a filtered subject sequence */
  Int4 length,                  /* length of the filtered sequence */
  Int4 ** matrix         /* a scoring matrix to be adjusted */
) {   

  double LambdaRatio;      /* the ratio of the corrected lambda to the 
                                 * original lambda */
  if(!sp->adjustParameters) {
    LambdaRatio = 1.0;
  } else {
    /* do adjust the parameters */
    Blast_ScoreFreq* this_sfp; 
    double correctUngappedLambda;  /* new value of ungapped lambda */
    Boolean positionBased=FALSE; /* FIXME */

    /* compute and plug in new matrix here */
    fillResidueProbability(filteredSequence, length, sp->resProb);

    if(positionBased) {
      this_sfp =
        posfillSfp(sp->startMatrix, queryLength, sp->resProb, sp->scoreArray,
                   sp->return_sfp, scoreRange);
    } else {
      this_sfp =
        notposfillSfp(sp->startMatrix, sp->resProb, sp->queryProb,
                      sp->scoreArray, sp->return_sfp, scoreRange);
    }
    correctUngappedLambda =
      Blast_KarlinLambdaNR(this_sfp, sp->scaledUngappedLambda);

    /* impalaKarlinLambdaNR will return -1 in the case where the
     * expected score is >=0; however, because of the MAX statement 3
     * lines below, LambdaRatio should always be > 0; the succeeding
     * test is retained as a vestige, in case one wishes to remove the
     * MAX statement and allow LambdaRatio to take on the error value
     * -1 */

    LambdaRatio = correctUngappedLambda / sp->scaledUngappedLambda;
    LambdaRatio = MIN(1, LambdaRatio);
    LambdaRatio = MAX(LambdaRatio, LambdaRatioLowerBound);

    if(LambdaRatio > 0) {
      scaleMatrix(matrix, sp->startMatrix, sp->startFreqRatios, sp->mRows,
                  sp->scaledUngappedLambda, LambdaRatio);
    }
  }
  /* end else do adjust the parameters */

  return LambdaRatio > 0 ? 0 : 1;
}


/** Restore the parameters that were adjusted to their original values
 * @param searchParams a record of the original values [in]
 * @param sbp Karlin-Altschul parameters to be restored. [out]
 * @param matrix the scoring matrix to be restored [out]
 * @param scoring the scoring parameters to be restored [out]
*/
static void
Kappa_RestoreSearch(
  Kappa_SearchParameters * searchParams, 
  BlastScoreBlk* sbp,	
  Int4 ** matrix,        
  BlastScoringParameters* scoring
) {
  if(searchParams->adjustParameters) {
    Blast_KarlinBlk* kbp;     /* statistical parameters used to
                                   evaluate the significance of
                                   alignment of a query-subject
                                   pair */
    Int4 i, j; /* loop variables. */
    Boolean positionBased=FALSE; /* FIXME. */

    scoring->gap_open = searchParams->gapOpen;
    scoring->gap_extend = searchParams->gapExtend;
    scoring->decline_align = searchParams->gapDecline;
    scoring->scale_factor = searchParams->scale_factor;

    sbp->kbp_gap       = searchParams->orig_kbp_gap_array;

    if(positionBased) {
      kbp = sbp->kbp_gap_psi[0];
    } else {
      kbp = sbp->kbp_gap_std[0];
    }
    kbp->Lambda = searchParams->kbp_gap_orig->Lambda;
    kbp->K      = searchParams->kbp_gap_orig->K;
    kbp->logK   = searchParams->kbp_gap_orig->logK;
    kbp->H      = searchParams->kbp_gap_orig->H;

    for(i = 0; i < searchParams->mRows; i++) {
      for(j = 0; j < BLASTAA_SIZE; j++) {
        matrix[i][j] = searchParams->origMatrix[i][j];
      }
    }
  }
}

/** Gets best expect value of the list
 *
 * @param hsplist the list to be examined [in]
 * @return the best (lowest) expect value found 
 */

static double
BlastHitsGetBestEvalue(BlastHSPList* hsplist)
{
    double retval = (double) INT4_MAX; /* return value */
    Int4 index; /* loop iterator */

    if (hsplist == NULL || hsplist->hspcnt == 0)
       return retval;

    for (index=0; index<hsplist->hspcnt; index++)
    {
         retval = MIN(retval, hsplist->hsp_array[index]->evalue);
    }

    return retval;
}

/** Save the results for one query, and clean the internal structure. */

static Int2
Blast_HSPResultsUpdateFromSWheap(SWheap* significantMatches,
                                 Int4 query_index, Int4 hitlist_size,
                                 BlastHSPResults* results)
{
   SWResults *SWAligns; /* All new alignments, concatenated
                           into a single, flat list */
   if (query_index < 0)
      return 0;

   SWAligns = SWheapToFlatList(significantMatches);

   results->hitlist_array[query_index] = Blast_HitListNew(hitlist_size);
          
   if(SWAligns != NULL) {
      newConvertSWalignsUpdateHitList(SWAligns, 
         results->hitlist_array[query_index]);
   }
   SWAligns = SWResultsFree(SWAligns);
   /* Clean up */
   SWheapRelease(significantMatches);
   return 0;
}

Int2
Kappa_RedoAlignmentCore(BLAST_SequenceBlk * queryBlk,
                  BlastQueryInfo* queryInfo,
                  BlastScoreBlk* sbp,
                  BlastHSPStream* hsp_stream,
                  const BlastSeqSrc* seqSrc,
                  BlastScoringParameters* scoringParams,
                  const BlastExtensionParameters* extendParams,
                  const BlastHitSavingParameters* hitsavingParams,
                  const PSIBlastOptions* psiOptions,
                  BlastHSPResults* results)
{

  const Uint1 k_program_name = blast_type_blastp;
  Boolean adjustParameters = FALSE; /* If true take match composition into account
                                                          and seg match sequence. */
  Boolean SmithWaterman = FALSE; /* USe smith-waterman to get scores.*/
  Boolean positionBased=FALSE; /* FIXME, how is this determined? */
  Int2 status=0;              /* Return value. */
  Uint1*    query;            /* the query sequence */
  Int4      queryLength;      /* the length of the query sequence */
  double localScalingFactor;       /* the factor by which to
                                         * scale the scoring system in
                                         * order to obtain greater
                                         * precision */

  Int4      **matrix;    /* score matrix */
  Blast_KarlinBlk* kbp;       /* stores Karlin-Altschul parameters */
  BlastGapAlignStruct*     gapAlign; /* keeps track of gapped alignment params */

  Kappa_SearchParameters *searchParams; /* the values of the search
                                         * parameters that will be
                                         * recorded, altered in the
                                         * search structure in this
                                         * routine, and then restored
                                         * before the routine
                                         * exits. */
  Kappa_ForbiddenRanges   forbidden;    /* forbidden ranges for each
                                         * database position (used in
                                         * Smith-Waterman alignments) */
  SWheap  significantMatches;  /* a collection of alignments of the
                                * query sequence with sequences from
                                * the database */

  BlastExtensionOptions* extendOptions=NULL; /* Options for extension. */
  BlastHitSavingOptions* hitsavingOptions=NULL; /* Options for saving hits. */
  BlastHSPList* thisMatch = NULL;  
  Int4 current_query_index;

  /* Get pointer to options for extensions and hitsaving. */
  if (extendParams == NULL || (extendOptions=extendParams->options) == NULL)
       return -1;

  if (hitsavingParams == NULL || (hitsavingOptions=hitsavingParams->options) == NULL)
       return -1;

  if (extendParams->options->eTbackExt ==  eSmithWatermanTbck)
    SmithWaterman = TRUE;

  adjustParameters = extendParams->options->compositionBasedStats; 

  sbp->kbp_ideal = Blast_KarlinBlkIdealCalc(sbp);


  /**** Validate parameters *************/
  if(0 == strcmp(scoringParams->options->matrix, "BLOSUM62_20") && !adjustParameters) {
    return 0;                   /* BLOSUM62_20 only makes sense if
                                 * adjustParameters is on */
  }
  /*****************/
  query = &queryBlk->sequence[0];
  queryLength = BLAST_GetQueryLength(queryInfo, 0);

  if(SmithWaterman) {
    Kappa_ForbiddenRangesInitialize(&forbidden, queryLength);
  }

  if ((status=BLAST_GapAlignStructNew(scoringParams, extendParams,
                    BLASTSeqSrcGetMaxSeqLen(seqSrc), queryLength, sbp,
                    &gapAlign)) != 0) 
      return status;

  if(positionBased) {
    kbp    = sbp->kbp_gap_psi[0];
    matrix = sbp->posMatrix;
    if(sbp->posFreqs == NULL) {
      sbp->posFreqs = (double**) _PSIAllocateMatrix(queryLength, BLASTAA_SIZE, sizeof(double));
    }
    } else {
    kbp    = sbp->kbp_gap_std[0];
    matrix = sbp->matrix;
  }

  /* Initialize searchParams */
  searchParams =
    Kappa_SearchParametersNew(queryLength, adjustParameters,
                              positionBased);
  Kappa_RecordInitialSearch(searchParams, queryBlk, queryInfo, sbp, scoringParams);

  localScalingFactor = Kappa_RescaleSearch(searchParams, queryBlk, queryInfo, sbp, scoringParams);

  /* Initialize current query index to -1, so index 0 would indicate a new 
     query. */
  current_query_index = -1;

  while (BlastHSPStreamRead(hsp_stream, &thisMatch) != kBlastHSPStream_Eof) {
    /* for all matching sequences */
    Kappa_MatchingSequence matchingSeq; /* the data for a matching
                                         * database sequence */

    if(thisMatch->hsp_array == NULL) {
      continue;
    }

    if (thisMatch->query_index != current_query_index) {
       /* This HSP list is for a new query sequence. Save results for 
          the previous query. */
       Blast_HSPResultsUpdateFromSWheap(&significantMatches, 
          current_query_index, hitsavingOptions->hitlist_size, results);
       SWheapInitialize(&significantMatches, hitsavingOptions->hitlist_size,
                        hitsavingOptions->hitlist_size, 
                        psiOptions->inclusion_ethresh);
       current_query_index = thisMatch->query_index;
    }

    if(SWheapWillAcceptOnlyBelowCutoff(&significantMatches)) {
      /* Only matches with evalue <= psiOptions->inclusion_ethresh will be saved */

      /* e-value for a sequence is the smallest e-value among the hsps
       * matching a region of the sequence to the query */
      double minEvalue = BlastHitsGetBestEvalue(thisMatch);  /* FIXME, do we have this on new structures? */
      if(minEvalue > (EVALUE_STRETCH * psiOptions->inclusion_ethresh)) {
        /* This match is likely to have an evalue > options->inclusion_ethresh
         * and therefore, we assume that all other matches with higher
         * input evalues are also unlikely to get sufficient
         * improvement in a redone alignment */
        break;
      }
    }
    /* Get the sequence for this match */
    Kappa_MatchingSequenceInitialize(&matchingSeq, seqSrc,
                                     thisMatch->oid);

    if(0 == Kappa_AdjustSearch(searchParams, queryLength,
                         matchingSeq.filteredSequence,
                         matchingSeq.length, matrix)) {
      /* Kappa_AdjustSearch ran without error. Compute the new alignments. */
      if(SmithWaterman) {
        /* We are performing a Smith-Waterman alignment */
        double newSwEvalue; /* the evalue computed by the SW algorithm */
        Int4 aSwScore;    /* a score computed by the SW algorithm */
        Int4 matchStart, queryStart;    /* Start positions of a local
                                         * S-W alignment */
        Int4 queryEnd, matchEnd;        /* End positions of a local
                                         * S-W alignment */

        Kappa_ForbiddenRangesClear(&forbidden);

        newSwEvalue =
          BLbasicSmithWatermanScoreOnly(matchingSeq.filteredSequence,
                                        matchingSeq.length, query,
                                        queryLength, matrix,
                                        scoringParams->gap_open,
                                        scoringParams->gap_extend, &matchEnd,
                                        &queryEnd, &aSwScore, kbp,
                                        queryInfo->eff_searchsp_array[0],
                                        positionBased);

        if(newSwEvalue <= hitsavingOptions->expect_value &&
           SWheapWouldInsert(&significantMatches, newSwEvalue ) ) {
          /* The initial local alignment is significant. Continue the
           * computation */
          Kappa_MatchRecord aSwMatch;   /* the newly computed
                                         * alignments of the query to
                                         * the current database
                                         * sequence */
        
          Kappa_MatchRecordInitialize(&aSwMatch, newSwEvalue, aSwScore,
                                      matchingSeq.sequence,
                                      thisMatch->oid);

          BLSmithWatermanFindStart(matchingSeq.filteredSequence,
                                   matchingSeq.length, query, matrix, 
                                   scoringParams->gap_open,
                                   scoringParams->gap_extend, matchEnd, queryEnd,
                                   aSwScore, &matchStart, &queryStart,
                                   positionBased);

          do {
            /* score computed by an x-drop alignment (usually the same
             * as aSwScore */
            Int4 newXdropScore;  
            /* Lengths of the alignment  as recomputed by an x-drop alignment,
               in the query and the match*/
            Int4 queryAlignmentExtent, matchAlignmentExtent;
            /* Alignment information (script) returned by a x-drop
             * alignment algorithm */
            Int4 *reverseAlignScript=NULL;   

            gapAlign->gap_x_dropoff =
              (Int4) (extendParams->gap_x_dropoff_final * localScalingFactor);

            Kappa_SWFindFinalEndsUsingXdrop(query, queryLength, queryStart,
                                            queryEnd,
                                            matchingSeq.filteredSequence,
                                            matchingSeq.length, matchStart,
                                            matchEnd, gapAlign, scoringParams,
                                            aSwScore, localScalingFactor,
                                            &queryAlignmentExtent, &matchAlignmentExtent,
                                            &reverseAlignScript,
                                            &newXdropScore);

            Kappa_MatchRecordInsertSwAlign(&aSwMatch, newXdropScore,
                                           newSwEvalue, kbp->Lambda,
                                           kbp->logK, localScalingFactor,
                                           matchStart, matchAlignmentExtent,
                                           queryStart, queryAlignmentExtent,
                                           reverseAlignScript, query);
            sfree(reverseAlignScript);

            Kappa_ForbiddenRangesPush(&forbidden, queryStart, queryAlignmentExtent,
                                      matchStart, matchAlignmentExtent);
            if(thisMatch->hspcnt > 1) {
              /* There are more HSPs */
              newSwEvalue =
                BLspecialSmithWatermanScoreOnly(matchingSeq.filteredSequence,
                                                matchingSeq.length, query,
                                                queryLength, matrix,
                                                scoringParams->gap_open,
                                                scoringParams->gap_extend,
                                                &matchEnd, &queryEnd,
                                                &aSwScore, kbp,
                                                queryInfo->eff_searchsp_array[0],
                                                forbidden.numForbidden,
                                                forbidden.ranges,
                                                positionBased);

              if(newSwEvalue <= hitsavingOptions->expect_value) {
                /* The next local alignment is significant */
                BLspecialSmithWatermanFindStart(matchingSeq.filteredSequence,
                                                matchingSeq.length, query,
                                                matrix,
                                                scoringParams->gap_open,
                                                scoringParams->gap_extend,
                                                matchEnd, queryEnd, aSwScore,
                                                &matchStart, &queryStart,
                                                forbidden.numForbidden,
                                                forbidden.ranges,
                                                positionBased);
              }
              /* end if the next local alignment is significant */
            }
            /* end if there are more HSPs */
          } while(thisMatch->hspcnt > 1 &&
                  newSwEvalue <= hitsavingOptions->expect_value);
          /* end do..while there are more HSPs and the next local alignment
           * is significant */

          SWheapInsert(&significantMatches, &aSwMatch);
        }
        /* end if the initial local alignment is significant */
      } else {
        /* We are not doing a Smith-Waterman alignment */
        gapAlign->gap_x_dropoff = 
          (Int4) (extendParams->gap_x_dropoff_final * localScalingFactor);
        /* recall that index is the counter corresponding to
         * thisMatch; by aliasing, thisMatch will get updated during
         * the following call to BlastGetGapAlgnTbck, so that
         * thisMatch stores newly computed alignments between the
         * query and the matching sequence number index */
          if ((status=Blast_TracebackFromHSPList(k_program_name, thisMatch, queryBlk, 
             matchingSeq.seq_blk, queryInfo, gapAlign, sbp, scoringParams, 
             extendOptions, hitsavingParams, NULL)) != 0)
             return status;

        if(thisMatch->hspcnt) {
          /* There are alignments of the query to this matching sequence */
          double bestEvalue = BlastHitsGetBestEvalue(thisMatch);  

          if(bestEvalue <= hitsavingOptions->expect_value &&
             SWheapWouldInsert(&significantMatches, bestEvalue ) ) {
            /* The best alignment is significant */
            Int4 alignIndex;            /* Iteration index */
            Int4 numNewAlignments;      /* the number of alignments
                                         * just computed */
            Kappa_MatchRecord matchRecord;      /* the newly computed
                                                 * alignments of the
                                                 * query to the
                                                 * current database
                                                 * sequence */
            Int4 bestScore;      /* the score of the highest
                                         * scoring alignment */
            numNewAlignments = thisMatch->hspcnt;  
            bestScore =
              (Int4) BLAST_Nint(((double) thisMatch->hsp_array[0]->score) /
                       localScalingFactor);

            Kappa_MatchRecordInitialize(&matchRecord, bestEvalue, bestScore,
                                        matchingSeq.sequence,
                                        thisMatch->oid);


            for(alignIndex = 0; alignIndex < numNewAlignments; alignIndex++) {
              Kappa_MatchRecordInsertHSP(&matchRecord, 
                                         &(thisMatch->hsp_array[alignIndex]),
                                         kbp->Lambda, kbp->logK,
                                         localScalingFactor, query);
            }
            /* end for all alignments of this matching sequence */
            SWheapInsert(&significantMatches, &matchRecord);
          }
          /* end if the best alignment is significant */
        }
        /* end there are alignments of the query to this matching sequence */
      }
      /* end else we are not doing a Smith-Waterman alignment */
    }
    /* end if Kappa_AdjustSearch ran without error */
    Kappa_MatchingSequenceRelease(&matchingSeq);
    
  }
  /* end for all matching sequences */

  /* Save results for the last query, which were not saved inside the loop. */
  Blast_HSPResultsUpdateFromSWheap(&significantMatches, 
     current_query_index, hitsavingOptions->hitlist_size, results);

  if(SmithWaterman) 
    Kappa_ForbiddenRangesRelease(&forbidden);
  Kappa_RestoreSearch(searchParams, sbp, matrix, scoringParams);

  Kappa_SearchParametersFree(&searchParams);

  gapAlign = BLAST_GapAlignStructFree(gapAlign);

  return 0;
}
