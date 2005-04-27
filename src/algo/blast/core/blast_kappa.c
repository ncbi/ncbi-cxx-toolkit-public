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

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_kappa.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/link_hsps.h>
#include "blast_psi_priv.h"
#include "matrix_freq_ratios.h"
#include "blast_gapalign_priv.h"
#include "blast_posit.h"
#include "blast_hits_priv.h"

/** by what factor might initially reported E-value exceed true Evalue */
#define EVALUE_STRETCH 5 

/** For translated subject sequences, the number of amino acids to
    include before and after the existing aligned segment when
    generating a composition-based scoring system. */
#define KAPPA_WINDOW_BORDER 200

/**
 * Scale the scores in an HSP list and reset the bit scores.
 *
 * @param hsp_list          the HSP list
 * @param logK              Karlin-Altschul statistical parameter [in]
 * @param lambda            Karlin-Altschul statistical parameter [in]
 * @param scoreDivisor      the value by which reported scores are to be
 * @todo rename to something which is more intention revealing, merge with
 * function of the same name in blast_traceback.c
 */
static void
s_HSPListRescaleScores(BlastHSPList * hsp_list,
                     double lambda,
                     double logK,
                     double scoreDivisor)
{
  int hsp_index;
  for(hsp_index = 0; hsp_index < hsp_list->hspcnt; hsp_index++) {
    BlastHSP * hsp = hsp_list->hsp_array[hsp_index];

    hsp->score = BLAST_Nint(((double) hsp->score) / scoreDivisor);
    /* Compute the bit score using the newly computed scaled score. */
    hsp->bit_score = (hsp->score*lambda*scoreDivisor - logK)/NCBIMATH_LN2;
  }
}

/**
 * Remove from a hitlist all HSPs that are completely contained in an
 * HSP that occurs earlier in the list and that:
 * - is on the same strand; and
 * - has equal or greater score.  T
 * The hitlist should be sorted by some measure of significance before
 * this routine is called.
 * @param hsp_array         array to be reaped
 * @param hspcnt            length of hsp_array
 */
static void
HitlistReapContained(
  BlastHSP * hsp_array[],
  Int4 * hspcnt)
{
  Int4 iread;       /* iteration index used to read the hitlist */
  Int4 iwrite;      /* iteration index used to write to the hitlist */
  Int4 old_hspcnt;  /* number of HSPs in the hitlist on entry */

  old_hspcnt = *hspcnt;

   for( iread = 1; iread < *hspcnt; iread++ ) {
     /* for all HSPs in the hitlist */
     Int4         ireadBack;    /* iterator over indices less than iread */
     BlastHSP    *hsp1;         /* an HSP that is a candidate for deletion */

     hsp1 = hsp_array[iread];
     for( ireadBack = 0; ireadBack < iread && hsp1 != NULL; ireadBack++ ) {
       /* for all HSPs before hsp1 in the hitlist and while hsp1 has not
          been deleted */
       BlastHSP *hsp2;          /* an HSP that occurs earlier in hsp_array
                                 * than hsp1 */
       hsp2 = hsp_array[ireadBack];

       if( hsp2 == NULL ) {  /* hsp2 was deleted in a prior iteration. */
         continue;
       }
       if(SIGN(hsp2->query.frame)   == SIGN(hsp1->query.frame) &&
          SIGN(hsp2->subject.frame) == SIGN(hsp1->subject.frame)) {
         /* hsp1 and hsp2 are in the same query/subject frame. */
         if(CONTAINED_IN_HSP
            (hsp2->query.offset,   hsp2->query.end,   hsp1->query.offset,
             hsp2->subject.offset, hsp2->subject.end,
             hsp1->subject.offset) &&
            CONTAINED_IN_HSP
            (hsp2->query.offset,   hsp2->query.end,   hsp1->query.end,
             hsp2->subject.offset, hsp2->subject.end,
             hsp1->subject.end)    &&
            hsp1->score <= hsp2->score) {
           hsp1 = hsp_array[iread] = Blast_HSPFree(hsp_array[iread]);
         }
       } /* end if hsp1 and hsp2 are in the same query/subject frame */
     } /* end for all HSPs before hsp1 in the hitlist */
   } /* end for all HSPs in the hitlist */

   /* Condense the hsp_array, removing any NULL items. */
   iwrite = 0;
   for( iread = 0; iread < *hspcnt; iread++ ) {
     if( hsp_array[iread] != NULL ) {
       hsp_array[iwrite++] = hsp_array[iread];
     }
   }
   *hspcnt = iwrite;
   /* Fill the remaining memory in hsp_array with NULL pointers. */
   for( ; iwrite < old_hspcnt; iwrite++ ) {
     hsp_array[iwrite] = NULL;
   }
}


/**
 * An object of type Kappa_DistinctAlignment represents a distinct
 * alignment of the query sequence to the current subject sequence.
 * These objects are typically part of a singly linked list of
 * distinct alignments, stored in the reverse of the order in which
 * they were computed.
 */
typedef struct Kappa_DistinctAlignment {
  Int4 score;            /**< the score of this alignment */
  Int4 queryStart;       /**< the start of the alignment in the query */
  Int4 queryEnd;         /**< one past the end of the alignment in the query */
  Int4 matchStart;       /**< the start of the alignment in the subject */
  Int4 matchEnd;         /**< one past the end of the alignment in the
                              subject */
  Int4 frame;            /**< the subject frame */
  GapEditScript * editScript;   /**< the alignment info for a gapped
                                   alignment */
  struct Kappa_DistinctAlignment * next;  /**< the next alignment in the
                                               list */
} Kappa_DistinctAlignment;


/**
 * Recursively free all alignments in the singly linked list whose
 * head is *palign. Set *palign to NULL.
 *
 * @param palign            pointer to the head of a singly linked list
 *                          of alignments.
 */
static void
Kappa_DistinctAlignmentsFree(Kappa_DistinctAlignment ** palign)
{
  Kappa_DistinctAlignment * align;      /* represents the current
                                           alignment in loops */
  align = *palign;  *palign = NULL;
  while(align != NULL) {
    /* Save the value of align->next, because align is to be deleted. */
    Kappa_DistinctAlignment * align_next = align->next;
    align_next = align->next;

    if(align->editScript) {
      GapEditScriptDelete(align->editScript);
    }
    sfree(align);

    align = align_next;
  }
}


/**
 * Converts a list of objects of type Kappa_DistinctAlignment to an
 * new object of type BlastHSPList and returns the result. Conversion
 * in this direction is lossless.  The list passed to this routine is
 * freed to ensure that there is no aliasing of fields between the
 * list of Kappa_DistinctAlignments and the new hitlist.
 *
 * @param alignments A list of distinct alignments; freed before return [in]
 * @param oid        Ordinal id of a database sequence [in]
 * @return Allocated and filled BlastHSPList strucutre.
 */
static BlastHSPList *
s_HSPListFromDistinctAlignments(
  Kappa_DistinctAlignment ** alignments,
  int oid)
{
  const int unknown_value = 0;
  BlastHSPList * hsp_list = Blast_HSPListNew(0);
  Kappa_DistinctAlignment * align;

  hsp_list->oid = oid;

  for(align = *alignments; NULL != align; align = align->next) {
    BlastHSP * new_hsp = NULL;

    Blast_HSPInit(align->queryStart,   align->queryEnd,
                  align->matchStart,   align->matchEnd,
                  unknown_value, unknown_value,
                  0, 0, align->frame, align->score,
                  &align->editScript, &new_hsp);

    /* At this point, the subject and possibly the query sequence have
     * been filtered; since it is not clear that num_ident of the
     * filtered sequences, rather than the original, is desired,
     * explictly leave num_ident blank. */
    new_hsp->num_ident = 0;

    Blast_HSPListSaveHSP(hsp_list, new_hsp);
  }
  Kappa_DistinctAlignmentsFree(alignments);
  Blast_HSPListSortByScore(hsp_list);

  return hsp_list;
}


/**
 * Given a list of alignments and a new alignment, create a new list
 * of alignments that conditionally includes the new alignment.
 *
 * If there is an equal or higher-scoring alignment in the preexisting
 * list of alignments that shares an endpoint with the new alignment,
 * then preexisting list is returned.  Otherwise, a new list is
 * returned with the new alignment as its head and the elements of
 * preexisting list that do not share an endpoint with the new
 * alignment as its tail. The order of elements is preserved.
 *
 * Typically, a list of alignments is built one alignment at a time
 * through a call to withDistinctEnds. All alignments in the resulting
 * list have distinct endpoints.  Which items are retained in the list
 * depends on the order in which they were added.
 *
 * Note that an endpoint is a triple, specifying a frame, a location
 * in the query and a location in the subject.  In other words,
 * alignments that are not in the same frame never share endpoints.
 *
 * @param p_newAlign        on input the alignment that may be added to
 *                          the list; on output NULL
 * @param p_oldAlignments   on input the existing list of alignments;
 *                          on output the new list
 */
static void
withDistinctEnds(
  Kappa_DistinctAlignment **p_newAlign,
  Kappa_DistinctAlignment **p_oldAlignments)
{
  /* Deference the input parameters. */
  Kappa_DistinctAlignment * newAlign      = *p_newAlign;
  Kappa_DistinctAlignment * oldAlignments = *p_oldAlignments;
  Kappa_DistinctAlignment * align;      /* represents the current
                                          alignment in loops */
  Boolean include_new_align;            /* true if the new alignment
                                           may be added to the list */
  *p_newAlign        = NULL;
  include_new_align  = 1;

  for(align = oldAlignments; align != NULL; align = align->next) {
    if(align->frame == newAlign->frame &&
       (   (   align->queryStart == newAlign->queryStart
            && align->matchStart == newAlign->matchStart)
        || (   align->queryEnd   == newAlign->queryEnd
            && align->matchEnd   == newAlign->matchEnd))) {
      /* At least one of the endpoints of newAlign matches an endpoint
         of align. */
      if( newAlign->score <= align->score ) {
        /* newAlign cannot be added to the list. */
        include_new_align = 0;
        break;
      }
    }
  }

  if(include_new_align) {
    Kappa_DistinctAlignment **tail;     /* tail of the list being created */

    tail  = &newAlign->next;
    align = oldAlignments;
    while(align != NULL) {
      /* Save align->next because align may be deleted. */
      Kappa_DistinctAlignment * align_next = align->next;
      align->next = NULL;
      if(align->frame == newAlign->frame &&
         (   (   align->queryStart == newAlign->queryStart
              && align->matchStart == newAlign->matchStart)
          || (   align->queryEnd   == newAlign->queryEnd
              && align->matchEnd   == newAlign->matchEnd))) {
        /* The alignment shares an end with newAlign; */
        /* delete the alignment. */
        Kappa_DistinctAlignmentsFree(&align);
      } else { /* The alignment does not share an end with newAlign; */
        /* add it to the output list. */
        *tail =  align;
        tail  = &align->next;
      }
      align = align_next;
    } /* end while align != NULL */
    *p_oldAlignments = newAlign;
  } else { /* do not include_new_align */
    Kappa_DistinctAlignmentsFree(&newAlign);
  } /* end else do not include newAlign */
}


/**
 * The number of bits by which the score of a previously computed
 * alignment must exceed the score of the HSP under consideration for
 * a containment relationship to be reported by the isAlreadyContained
 * routine. */
#define KAPPA_BIT_TOL 2


/**
 * Return true if the HSP is already contained in a
 * previously-computed alignment of sufficiently high score.
 *
 * @param hsp                 HSP to be tested
 * @param alignments          list of alignments
 * @param lambda              Karlin-Altschul statistical parameter
 * @param localScalingFactor  factor by which scores were scaled to
 *                            obtain higher precision
 */

static Boolean
isAlreadyContained(
  BlastHSP * hsp,
  Kappa_DistinctAlignment * alignments,
  double lambda,
  double localScalingFactor)
{
  Kappa_DistinctAlignment * align;     /* represents the current alignment
                                          in the main loop */
  double scoreTol;   /* the amount by which the score of the current
                             alignment must exceed the score of the HSP for a
                             containment relationship to be reported. */
  scoreTol = KAPPA_BIT_TOL * NCBIMATH_LN2/lambda;

  for( align = alignments; align != NULL; align = align->next ) {
    /* for all elements of alignments */
    if(SIGN(hsp->query.frame)   == SIGN(align->frame)) {
      /* hsp1 and hsp2 are in the same query/subject frame */
      if(CONTAINED_IN_HSP
         (align->queryStart, align->queryEnd, hsp->query.offset,
          align->matchStart, align->matchEnd, hsp->subject.offset) &&
         CONTAINED_IN_HSP
         (align->queryStart, align->queryEnd, hsp->query.end,
          align->matchStart, align->matchEnd, hsp->subject.end) &&
                 hsp->score * localScalingFactor + scoreTol <= align->score) {
        return 1;
      }
    } /* hsp1 and hsp2 are in the same query/subject frame */
  } /* end for all items in alignments */

  return 0;
}


/**
 * The struct SWheapRecord data type is used below to define the
 * internal structure of a SWheap (see below).  A SWheapRecord
 * represents all alignments of a query sequence to a particular
 * matching sequence.
 */
typedef struct SWheapRecord {
  double   bestEvalue;     /**< best (smallest) evalue of all alignments
                                     in the record */
  Int4          subject_index;  /**< index of the subject sequence in
                                     the database */
  BlastHSPList * theseAlignments;  /**< a list of alignments */
} SWheapRecord;


/** Compare two records in the heap.  */
static Boolean
SWheapRecordCompare(SWheapRecord * place1,
                    SWheapRecord * place2)
{
  return ((place1->bestEvalue    >  place2->bestEvalue) ||
          (place1->bestEvalue    == place2->bestEvalue &&
           place1->subject_index >  place2->subject_index));
}


/** swap two records in the heap*/
static void
SWheapRecordSwap(SWheapRecord * heapArray,
                 Int4 i,
                 Int4 j)
{
  /* bestEvalue, theseAlignments and subject_index are temporary
   * variables used to perform the swap. */
  double bestEvalue       = heapArray[i].bestEvalue;
  BlastHSPList * theseAlignments  = heapArray[i].theseAlignments;
  Int4        subject_index    = heapArray[i].subject_index;

  heapArray[i].bestEvalue      = heapArray[j].bestEvalue;
  heapArray[i].theseAlignments = heapArray[j].theseAlignments;
  heapArray[i].subject_index   = heapArray[j].subject_index;

  heapArray[j].bestEvalue      = bestEvalue;
  heapArray[j].theseAlignments = theseAlignments;
  heapArray[j].subject_index   = subject_index;
}


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

/** convenience debugging macro for this module */
#ifdef KAPPA_INTENSE_DEBUG
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
 * @param i element of heapArray that may be out of order [in]
 * @param n size of heapArray [in]
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


/** On entry, all but the last element of the array heapArray[0]
 * .. heapArray[i] are in valid heap order.  This routine rearranges
 * the elements so that on exit they all are in heap order.
 *
 * @param heapArray      holds the heap [in][out]
 * @param i              element in heap array that may be out of order [in]
 * @param n              size of heapArray
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

/**
 * A SWheap represents a collection of alignments between one query
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
typedef struct SWheap {
  Int4 n;                       /**< The current number of elements */
  Int4 capacity;                /**< The maximum number of elements
                                     that may be inserted before the
                                     SWheap must be resized */
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

} SWheap;


/** Convert a SWheap from a representation as an unordered array to
 *  a representation as a heap-ordered array.
 *
 *  @param self         the SWheap to convert
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

/** When the heap is about to exceed its capacity, it will be grown by
 * the minimum of a multiplicative factor of SWHEAP_RESIZE_FACTOR
 * and an additive factor of SWHEAP_MIN_RESIZE. The heap never
 * decreases in size */
#define SWHEAP_RESIZE_FACTOR 1.5
/** @sa SWHEAP_RESIZE_FACTOR */
#define SWHEAP_MIN_RESIZE 100

/** Return true if self would insert a match that had the given eValue
 *  @param self         a SWheap
 *  @param eValue       the evalue to be tested.
 */
static Boolean
SWheapWouldInsert(SWheap * self,
                  double eValue)
{
  return self->n < self->heapThreshold ||
    eValue <= self->ecutoff ||
    eValue < self->worstEvalue;
}


/**
 * Try to insert matchRecord into the SWheap. The list of SeqAligns
 * passed to this routine is used directly, i.e. the list is not copied,
 * but is rather stored in the SWheap or deleted.
 *
 * @param self              the heap
 * @param alignments        a list of alignments
 * @param eValue            the best evalue among the alignments
 * @param subject_index     the index of the subject sequence in the database
 */
static void
SWheapInsert(
  SWheap * self,
  BlastHSPList * alignments,
  double eValue,
  Int4 subject_index)
{
  if(self->array && self->n >= self->heapThreshold) {
    ConvertToHeap(self);
  }
  if(self->array != NULL) {
    /* "self" is currently a list. Add the new alignments to the end */
    SWheapRecord *heapRecord;   /* destination for the new alignments */
    heapRecord                  = &self->array[++self->n];
    heapRecord->bestEvalue      = eValue;
    heapRecord->theseAlignments = alignments;
    heapRecord->subject_index   = subject_index;
    if( self->worstEvalue < eValue ) {
      self->worstEvalue = eValue;
    }
  } else {                      /* "self" is currently a heap */
    if(self->n < self->heapThreshold ||
       (eValue <= self->ecutoff &&
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
      heapRecord->bestEvalue      = eValue;
      heapRecord->theseAlignments = alignments;
      heapRecord->subject_index   = subject_index;

      SWheapifyUp(self->heapArray, self->n, self->n);
    } else {
      /* Some set of alignments must be discarded; discardedAlignments
       * will hold a pointer to these alignments. */
      BlastHSPList * discardedAlignments = NULL;

      if(eValue >= self->worstEvalue) {
        /* The new alignments must be discarded. */
        discardedAlignments = alignments;
      } else {
        /* The largest element in the heap must be discarded. */
        SWheapRecord *heapRecord;     /* destination for the new alignments */
        discardedAlignments         = self->heapArray[1].theseAlignments;

        heapRecord                  = &self->heapArray[1];
        heapRecord->bestEvalue      = eValue;
        heapRecord->theseAlignments = alignments;
        heapRecord->subject_index   = subject_index;

        SWheapifyDown(self->heapArray, 1, self->n);
      }
      /* end else the largest element in the heap must be discarded */
      if(discardedAlignments != NULL) {
        Blast_HSPListFree(discardedAlignments);
      }
      /* end while there are discarded alignments that have not been freed */
    }
    /* end else some set of alignments must be discarded */

    self->worstEvalue = self->heapArray[1].bestEvalue;
    KAPPA_ASSERT(SWheapIsValid(self->heapArray, 1, self->n));
  }
  /* end else "self" is currently a heap. */
}


/**
 * Return true if only matches with evalue <= self->ecutoff may be
 * inserted.
 *
 * @param self          a SWheap
 */
static Boolean
SWheapWillAcceptOnlyBelowCutoff(SWheap * self)
{
  return self->n >= self->heapThreshold && self->worstEvalue <= self->ecutoff;
}


/** Initialize a new SWheap; parameters to this function correspond
 * directly to fields in the SWheap */
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
    (SWheapRecord *) calloc(capacity + 1, sizeof(SWheapRecord));
  self->capacity      = capacity;
}


/**
 * Release the storage associated with the fields of a SWheap. Don't
 * delete the SWheap structure itself.
 *
 * @param self          SWheap whose storage will be released
 */
static void
SWheapRelease(SWheap * self)
{
  if(self->heapArray) sfree(self->heapArray);
  if(self->array) sfree(self->array);

  self->n = self->capacity = self->heapThreshold = 0;
}


/**
 * Remove and return the element in the SWheap with largest (worst) evalue
 *
 * @param self           a SWheap
 */
static BlastHSPList *
SWheapPop(SWheap * self)
{
  BlastHSPList * results = NULL;   /* the list of HSPs to be returned */

  ConvertToHeap(self);
  if(self->n > 0) { /* The heap is not empty */
    SWheapRecord *first, *last; /* The first and last elements of the
                                 * array that represents the heap. */
    first = &self->heapArray[1];
    last  = &self->heapArray[self->n];

    results = first->theseAlignments;

    first->theseAlignments = last->theseAlignments;
    first->bestEvalue      = last->bestEvalue;
    first->subject_index   = last->subject_index;

    SWheapifyDown(self->heapArray, 1, --self->n);
  }

  KAPPA_ASSERT(SWheapIsValid(self->heapArray, 1, self->n));

  return results;
}


/**
 * Convert a SWheap to a flat list of SeqAligns. Note that there may
 * be more than one alignment per element in the heap.  The new list
 * preserves the order of the SeqAligns associated with each
 * HeapRecord. (@todo this function is named as it is for compatibility with
 * kappa.c, rename in the future)
 *
 * @param self           a SWheap
 * @param results        BLAST core external results structure (pre-SeqAlign)
 *                       [out]
 * @param hitlist_size   size of each list in the results structure above [in]
 */
static void
SWheapToFlatList(SWheap * self, BlastHSPResults * results, Int4 hitlist_size)
{
  BlastHSPList* hsp_list;
  BlastHitList* hitlist =
    results->hitlist_array[0] = Blast_HitListNew(hitlist_size);

  hsp_list = NULL;
  while(NULL != (hsp_list = SWheapPop(self))) {
    Blast_HitListUpdate(hitlist, hsp_list);
  }
}


/** keeps one row of the Smith-Waterman matrix
 */
typedef struct SWpairs {
  Int4 noGap;       /**< @todo document me */
  Int4 gapExists;   /**< @todo document me */
} SWpairs;


/**
 * computes Smith-Waterman local alignment score and returns the
 * evalue
 *
 * @param matchSeq          is a database sequence matched by this query [in]
 * @param matchSeqLength    is the length of matchSeq in amino acids [in]
 * @param query             is the input query sequence [in]
 * @param queryLength       is the length of query [in]
 * @param matrix            is the position-specific matrix associated with
 *                          query [in]
 * @param gapOpen           is the cost of opening a gap [in]
 * @param gapExtend         is the cost of extending an existing gap by 1
 *                          position [in]
 * @param matchSeqEnd       returns the final position in the matchSeq of an
 *                          optimal local alignment [in]
 * @param queryEnd          returns the final position in query of an optimal
 *                          local alignment [in]. matchSeqEnd and queryEnd can 
 *                          be used to run the local alignment
 *                          in reverse to find optimal starting positions [in]
 * @param score             is used to pass back the optimal score [in]
 * @param kbp               holds the Karlin-Altschul parameters [in]
 * @param effSearchSpace    effective search space [in]
 * @param positionSpecific  determines whether matrix is position
 *                          specific or not [in]
 * @return                  the expect value of the alignment
 */
static double
BLbasicSmithWatermanScoreOnly(Uint1 * matchSeq,
   Int4 matchSeqLength, Uint1 *query, Int4 queryLength,
   Int4 **matrix,
   Int4 gapOpen, Int4 gapExtend,  Int4 *matchSeqEnd, Int4 *queryEnd,
   Int4 *score,
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


   scoreVector = (SWpairs *) calloc(matchSeqLength, sizeof(SWpairs));
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


/**
 * computes where optimal Smith-Waterman local alignment starts given
 * the ending positions and score matchSeqEnd and queryEnd can be used
 * to run the local alignment in reverse to find optimal starting
 * positions these are passed back in matchSeqStart and queryStart the
 * optimal score is passed in to check when it has been reached going
 * backwards the score is also returned
 * @param matchSeq          is a database sequence matched by this query [in]
 * @param matchSeqLength    is the length of matchSeq in amino acids [in]
 * @param query             is the input query sequence  [in]
 * @param matrix            is the position-specific matrix associated with
 *                          query or the standard matrix [in]
 * @param gapOpen           is the cost of opening a gap [in]
 * @param gapExtend         is the cost of extending an existing gap by 1
 *                          position [in]
 * @param matchSeqEnd       is the final position in the matchSeq of an optimal
 *                          local alignment [in]
 * @param queryEnd          is the final position in query of an optimal
 *                          local alignment [in]
 * @param score optimal     score to be obtained [in]
 * @param matchSeqStart     starting point of optimal alignment [out]
 * @param queryStart        starting point of optimal alignment [out]
 * @param positionSpecific  determines whether matrix is position specific
 *                          or not
 */
static Int4
BLSmithWatermanFindStart(Uint1 * matchSeq,
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

   scoreVector = (SWpairs *) calloc(matchSeqLength, sizeof(SWpairs));
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


/**
 * computes Smith-Waterman local alignment score and returns the
 * evalue assuming some positions are forbidden matchSeqEnd and query
 * can be used to run the local alignment in reverse to find optimal
 * starting positions
 * @param                   matchSeq is the matchSeq sequence [in]
 * @param matchSeqLength    is the length of matchSeq in amino acids [in]
 * @param query             is the input query sequence  [in]
 * @param queryLength       is the length of query [in]
 * @param matrix            is either the position-specific matrix associated
 *                          with query or the standard matrix [in]
 * @param gapOpen           is the cost of opening a gap [in]
 * @param gapExtend         is the cost of extending an existing gap by 1
 *                          position [in]
 * @param matchSeqEnd       returns the final position in the matchSeq of an
 *                          optimal local alignment [in]
 * @param queryEnd          returns the final position in query of an optimal
 *                          local alignment [in]
 * @param score             is used to pass back the optimal score [out]
 * @param kbp               holds the Karlin-Altschul parameters  [in]
 * @param effSearchSpace    effective search space [in]
 * @param numForbidden      number of forbidden ranges [in]
 * @param forbiddenRanges   lists areas that should not be aligned [in]
 * @param positionSpecific  determines whether matrix is position specific
 *                          or not [in]
 */
static double
BLspecialSmithWatermanScoreOnly(Uint1 * matchSeq,
   Int4 matchSeqLength, Uint1 *query, Int4 queryLength, Int4 **matrix,
   Int4 gapOpen, Int4 gapExtend,
   Int4 *matchSeqEnd, Int4 *queryEnd, Int4 *score,
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


/**
 * computes where optimal Smith-Waterman local alignment starts given
 * the ending positions.  matchSeqEnd and queryEnd can be used to run
 * the local alignment in reverse to find optimal starting positions
 * these are passed back in matchSeqStart and queryStart the optimal
 * score is passed in to check when it has been reached going
 * backwards the score is also returned
 * @param matchSeq          is the matchSeq sequence [in]
 * @param matchSeqLength    is the length of matchSeq in amino acids [in]
 * @param query             is the sequence corresponding to some matrix
 *                          profile [in]
 * @param matrix            is the position-specific matrix associated with
 *                          query [in]
 * @param gapOpen           is the cost of opening a gap [in]
 * @param gapExtend         is the cost of extending an existing gap by 1
 *                          position [in]
 * @param matchSeqEnd       is the final position in the matchSeq of an optimal
 *                          local alignment [in]
 * @param queryEnd          is the final position in query of an optimal
 *                          local alignment [in]
 * @param score             optimal score is passed in to check when it has
 *                          been reached going backwards [in]
 * @param matchSeqStart     optimal starting point [in]
 * @param queryStart        optimal starting point [in]
 * @param numForbidden      array of regions not to be aligned. [in]
 * @param numForbidden      array of regions not to be aligned. [in]
 * @param forbiddenRanges   regions not to be aligned. [in]
 * @param positionSpecific  determines whether matrix is position specific
 *                          or not
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

   scoreVector = (SWpairs *) calloc(matchSeqLength, sizeof(SWpairs));
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


/**
 * Kappa_SequenceData - represents a string of amino acids or nucleotides
 */
typedef struct Kappa_SequenceData {
  Uint1 *data;                  /**< amino acid or nucleotide data */
  Int4 length;                  /**< the length of data. For amino acid data
                                     &data[-1] is a valid address and
                                     data[-1] == 0. */
  Uint1 *buffer;                /**< if non-nil, points to memory that
                                     must be freed when this instance of
                                     Kappa_SequenceData is deleted. */
} Kappa_SequenceData;


/** Release the data associated with this object. */
static void
Kappa_SequenceDataRelease(Kappa_SequenceData * self)
{
  if(self->buffer) sfree(self->buffer);

  self->data   = NULL;
  self->buffer = NULL;
}


/**
 * An instance of Kappa_ForbiddenRanges is used by the Smith-Waterman
 * algorithm to represent ranges in the database that are not to be
 * aligned.
 */
typedef struct Kappa_ForbiddenRanges {
  Boolean  isEmpty;             /**< True if there are no forbidden ranges */
  Int4    *numForbidden;        /**< how many forbidden ranges at each db
                                     position */
  Int4   **ranges;              /**< forbidden ranges for each database
                                     position */
  Int4     queryLength;         /**< length of the query sequence */
} Kappa_ForbiddenRanges;


/**
 * Initialize a new, empty Kappa_ForbiddenRanges
 *
 * @param self              object to be initialized
 * @param queryLength       the length of the query
 */
static void
Kappa_ForbiddenRangesInitialize(
  Kappa_ForbiddenRanges * self,
  Int4 queryLength)
{
  Int4 f;
  self->queryLength  = queryLength;
  self->numForbidden = (Int4 *) malloc(queryLength * sizeof(Int4));
  self->ranges       = (Int4 **) malloc(queryLength * sizeof(Int4 *));
  self->isEmpty      = TRUE;

  for(f = 0; f < queryLength; f++) {
    self->numForbidden[f] = 0;
    self->ranges[f]       = (Int4 *) malloc(2 * sizeof(Int4));
    self->ranges[f][0]    = 0;
    self->ranges[f][1]    = 0;
  }
}


/** Reset self to be empty */
static void
Kappa_ForbiddenRangesClear(Kappa_ForbiddenRanges * self)
{
  Int4 f;
  for(f = 0; f < self->queryLength; f++) {
    self->numForbidden[f] = 0;
  }
  self->isEmpty = TRUE;
}


/** Add some ranges to self
 * @param self          an instance of Kappa_ForbiddenRanges [in][out]
 * @param queryStart    start of the alignment in the query sequence
 * @param queryAlignmentExtent  length of the alignment in the query sequence
 * @param matchStart    start of the alignment in the subject sequence
 * @param matchAlignmentExtent  length of the alignment in the
 *                              subject sequence
 */
static void
Kappa_ForbiddenRangesPush(
  Kappa_ForbiddenRanges * self,
  Int4 queryStart,
  Int4 queryAlignmentExtent,
  Int4 matchStart,
  Int4 matchAlignmentExtent)
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
  self->isEmpty = FALSE;
}


/**
 * Release the storage associated with the fields of self, but do not
 * delete self
 *
 * @param self          an instance of Kappa_ForbiddenRanges [in][out]
 */
static void
Kappa_ForbiddenRangesRelease(Kappa_ForbiddenRanges * self)
{
  Int4 f;
  for(f = 0; f < self->queryLength; f++)  sfree(self->ranges[f]);

  sfree(self->ranges);       self->ranges       = NULL;
  sfree(self->numForbidden); self->numForbidden = NULL;
}


/**
 * Calls BLbasicSmithWatermanScoreOnly if forbiddenRanges is empty and
 * calls BLspecialSmithWatermanScoreOnly otherwise.  This routine has
 * the same parameters and return value as
 * BLspecialSmithWatermanScoreOnly.
 */
static double
SmithWatermanScoreOnly(Kappa_SequenceData * subject,
                       Kappa_SequenceData * query,
                       Int4 **matrix,
                       Int4 gapOpen,
                       Int4 gapExtend,
                       Int4 *matchSeqEnd,
                       Int4 *queryEnd,
                       Int4 *score,
                       Blast_KarlinBlk * kbp,
                       Int8 effSearchSpace,
                       Boolean positionSpecific,
                       Kappa_ForbiddenRanges * forbiddenRanges )
{
  if( forbiddenRanges->isEmpty ) {
    return
      BLbasicSmithWatermanScoreOnly(subject->data, subject->length,
                                    query  ->data, query  ->length,
                                    matrix, gapOpen, gapExtend, matchSeqEnd,
                                    queryEnd, score, kbp, effSearchSpace,
                                    positionSpecific);
  } else {
    return
      BLspecialSmithWatermanScoreOnly(subject->data, subject->length,
                                      query  ->data, query  ->length,
                                      matrix, gapOpen, gapExtend, matchSeqEnd,
                                      queryEnd, score, kbp, effSearchSpace,
                                      forbiddenRanges->numForbidden,
                                      forbiddenRanges->ranges,
                                      positionSpecific);
  }
}


/**
 * Calls BLSmithWatermanFindStart if forbiddenRanges is empty and
 * calls BLspecialSmithWatermanFindStart otherwise.  This routine has
 * the same parameters and return value as
 * BLspecialSmithWatermanFindStart.
 */
static Int4
SmithWatermanFindStart(Kappa_SequenceData * subject,
                       Kappa_SequenceData * query,
                       Int4 **matrix,
                       Int4 gapOpen,
                       Int4 gapExtend,
                       Int4 matchSeqEnd,
                       Int4 queryEnd,
                       Int4 score,
                       Int4 *matchSeqStart,
                       Int4 *queryStart,
                       Boolean positionSpecific,
                       Kappa_ForbiddenRanges * forbiddenRanges)
{
  if( forbiddenRanges->isEmpty ) {
    return
      BLSmithWatermanFindStart(subject->data, subject->length,
                               query  ->data,
                               matrix, gapOpen, gapExtend,
                               matchSeqEnd,   queryEnd,   score,
                               matchSeqStart, queryStart,
                               positionSpecific);
  } else {
    return
      BLspecialSmithWatermanFindStart(subject->data, subject->length,
                                      query  ->data,
                                      matrix, gapOpen, gapExtend,
                                      matchSeqEnd,   queryEnd,   score,
                                      matchSeqStart, queryStart,
                                      forbiddenRanges->numForbidden,
                                      forbiddenRanges->ranges,
                                      positionSpecific);
  }
}


/**
 * @param matrix          is a position-specific score matrix with matrixLength
 *                        positions
 * @param subjectProbArray  is an array containing the probability of
 *                        occurrence of each residue in the subject
 * @param queryProbArray  is an array containing the probability of
 *                        occurrence of each residue in the query
 * @param scoreArray      is an array of probabilities for each score that is
 *                        to be used as a field in return_sfp
 * @param return_sfp      is a the structure to be filled in and returned
 * @param range           is the size of scoreArray and is an upper bound on
 *                        the difference between maximum score and minimum
 *                        score in the matrix
 * the routine posfillSfp computes the probability of each score
 * weighted by the probability of each query residue and fills those
 * probabilities into scoreArray and puts scoreArray as a field in
 * that in the structure that is returned for indexing convenience the
 * field storing scoreArray points to the entry for score 0, so that
 * referring to the -k index corresponds to score -k
 */
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


/**
 *
 * @param matrix            is a position-specific score matrix with
 *                          matrixLength positions
 * @param matrixLength      length of the position-specific matrix above
 * @param subjectProbArray  is an array containing the probability of
 *                          occurrence of each residue in the matching
 *                          sequence often called the subject
 * @param  scoreArray       is an array of probabilities for each score
 *                          that is to be used as a field in return_sfp
 * @param return_sfp        is a the structure to be filled in and returned
 *                          range is the size of scoreArray and is an upper
 *                          bound on the difference between maximum score
 *                          and minimum score in the matrix
 * @param range             is the size of scoreArray and is an upper bound on
 *                          the difference between maximum score and minimum
 *                          score in the matrix
 * the routine posfillSfp computes the probability of each score
 * weighted by the probability of each query residue and fills those
 * probabilities into scoreArray and puts scoreArray as a field in
 * that in the structure that is returned for indexing convenience the
 * field storing scoreArray points to the entry for score 0, so that
 * referring to the -k index corresponds to score -k
 */
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
   double** returnRatios; /*frequency ratios to start investigating each pair*/
   double *standardProb; /*probabilities of each letter*/
   Int4 i,j;  /* Loop indices. */
   SFreqRatios* freqRatios=NULL; /* frequency ratio container for given matrix */
   const double kPosEpsilon = 0.0001;

   returnRatios = (double**) _PSIAllocateMatrix(numPositions, 
                                                BLASTAA_SIZE, 
                                                sizeof(double));

   freqRatios = _PSIMatrixFrequencyRatiosNew(matrixName);
   if (freqRatios == NULL)
	return NULL;

   for(i = 0; i < numPositions; i++) {
     for(j = 0; j < BLASTAA_SIZE; j++) {
	   returnRatios[i][j] = freqRatios->data[query[i]][j];
     }
   }

   freqRatios = _PSIMatrixFrequencyRatiosFree(freqRatios);

   standardProb = BLAST_GetStandardAaProbabilities();

   /*reverse multiplication done in posit.c*/
   for(i = 0; i < numPositions; i++)
     for(j = 0; j < BLASTAA_SIZE; j++)
       if ((standardProb[query[i]] > kPosEpsilon) && (standardProb[j] > kPosEpsilon) &&
             (j != AMINOACID_TO_NCBISTDAA['X']) && (j != AMINOACID_TO_NCBISTDAA['*'])
             && (startNumerator[i][j] > kPosEpsilon))
           returnRatios[i][j] = startNumerator[i][j]/standardProb[j];

   sfree(standardProb);

   return(returnRatios);
}


/**
 * take every entry of startFreqRatios that is not corresponding to a
 * score of BLAST_SCORE_MIN and take its log, divide by Lambda and
 * multiply by LambdaRatio then round to the nearest integer and put
 * the result in the corresponding entry of matrix. startMatrix and
 * matrix have dimensions numPositions X BLASTAA_SIZE
 *
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

/** SCALING_FACTOR is a multiplicative factor used to get more bits of
 * precision in the integer matrix scores. It cannot be arbitrarily
 * large because we do not want total alignment scores to exceedto
 * -(BLAST_SCORE_MIN) */
#define SCALING_FACTOR 32


/**
 * Compute a scaled up version of the standard matrix encoded by
 * matrix name.  Standard matrices are in half-bit units.
 *
 * @param matrix preallocated matrix [in][out]
 * @param matrixName name of matrix (e.g., BLOSUM62, PAM30). [in]
 * @param Lambda A Karlin-Altschul parameter. [in]
 */
static void
computeScaledStandardMatrix(
  Int4 **matrix,
  char *matrixName,
  double Lambda)
{
   int i,j; /*loop indices*/
   SFreqRatios* freqRatios=NULL;  /* frequency ratios for the matrix */

   freqRatios = _PSIMatrixFrequencyRatiosNew(matrixName);
   ASSERT(freqRatios);
   if (freqRatios == NULL)
	return;

   for(i = 0; i < BLASTAA_SIZE; i++)
     for(j = 0; j < BLASTAA_SIZE; j++) {
         if(0.0 == freqRatios->data[i][j])
	   matrix[i][j] = BLAST_SCORE_MIN;
	 else {
	   double temp = log(freqRatios->data[i][j])/Lambda;
       matrix[i][j] = BLAST_Nint(temp);
     }
   }

   freqRatios = _PSIMatrixFrequencyRatiosFree(freqRatios);
}


/**
 * produce a scaled-up version of the position-specific matrix
 * starting from posFreqs
 *
 * @param fillPosMatrix     is the matrix to be filled
 * @param nonposMatrix      is the underlying position-independent matrix,
 *                          used to fill positions where frequencies are
 *                          irrelevant
 * @param sbp               stores various parameters of the search
 */
static int
scalePosMatrix(int **fillPosMatrix, 
               int **nonposMatrix, 
               const char *matrixName, 
               double **posFreqs, 
               Uint1 *query, 
               int queryLength, 
               BlastScoreBlk* sbp)
{
    Kappa_posSearchItems *posSearch = NULL;
    Kappa_compactSearchItems *compactSearch = NULL;
    _PSIInternalPssmData* internal_pssm = NULL;
    int status = PSI_SUCCESS;

    posSearch = Kappa_posSearchItemsNew(queryLength, matrixName, 
                                        fillPosMatrix, posFreqs);
    compactSearch = Kappa_compactSearchItemsNew(query, queryLength, sbp);

    /* Copy data into new structures */
    internal_pssm = _PSIInternalPssmDataNew(queryLength, BLASTAA_SIZE);
    _PSICopyMatrix_int(internal_pssm->pssm, posSearch->posMatrix,
                       internal_pssm->ncols, internal_pssm->nrows);
    _PSICopyMatrix_int(internal_pssm->scaled_pssm, posSearch->posPrivateMatrix,
                       internal_pssm->ncols, internal_pssm->nrows);
    _PSICopyMatrix_double(internal_pssm->freq_ratios, posSearch->posFreqs,
                          internal_pssm->ncols, internal_pssm->nrows);
    status = _PSIConvertFreqRatiosToPSSM(internal_pssm, query, sbp, 
                                         compactSearch->standardProb);
    if (status != PSI_SUCCESS) {
        internal_pssm = _PSIInternalPssmDataFree(internal_pssm);
        posSearch = Kappa_posSearchItemsFree(posSearch);
        compactSearch = Kappa_compactSearchItemsFree(compactSearch);
        return status;
    }

    /* Copy data from new structures to posSearchItems */
    _PSICopyMatrix_int(posSearch->posMatrix, internal_pssm->pssm,
                       internal_pssm->ncols, internal_pssm->nrows);
    _PSICopyMatrix_int(posSearch->posPrivateMatrix, internal_pssm->scaled_pssm,
                       internal_pssm->ncols, internal_pssm->nrows);
    _PSICopyMatrix_double(posSearch->posFreqs, internal_pssm->freq_ratios,
                          internal_pssm->ncols, internal_pssm->nrows);
    status = Kappa_impalaScaling(posSearch, 
                           compactSearch, 
                           (double) SCALING_FACTOR, 
                           FALSE, 
                           sbp);
    if (status != 0) {
        internal_pssm = _PSIInternalPssmDataFree(internal_pssm);
        posSearch = Kappa_posSearchItemsFree(posSearch);
        compactSearch = Kappa_compactSearchItemsFree(compactSearch);
        return status;
    }

    internal_pssm = _PSIInternalPssmDataFree(internal_pssm);
    posSearch = Kappa_posSearchItemsFree(posSearch);
    compactSearch = Kappa_compactSearchItemsFree(compactSearch);
    return status;
}


/**
 * Kappa_WindowInfo - a struct whose instances represent a range
 * of data in a sequence. */
typedef struct Kappa_WindowInfo
{
  Int4 begin;  /**< the starting index of the range */
  Int4 end;    /**< one beyond the last item in the range */
  Int4 frame;  /**< the translation frame of this window */
  Int4 hspcnt; /**< the number of HSPs aligned to a subset of the data
                    in this window's range. */
} Kappa_WindowInfo;


/**
 * A datatype used solely to enable a list of windows and of indices
 * to be simultaneously sorted in the WindowsFromHSPs routine.
 */
typedef struct Kappa_WindowIndexPair {
  Kappa_WindowInfo * window;    /**< a window */
  Int4 index;                   /**< an index associated with
                                     "window," typically the index of
                                     the window in a list, before the
                                     list is sorted. */
} Kappa_WindowIndexPair;

/**
 * A comparison routine used to sort a list of Kappa_WindowIndexPair
 * objects first by frame and then by location.
 */
static int
location_compare_windows(const void * vp1, const void *vp2)
{
  /* w1 and w2 are the windows being compared */
  Kappa_WindowInfo * w1 = ((Kappa_WindowIndexPair *) vp1)->window;
  Kappa_WindowInfo * w2 = ((Kappa_WindowIndexPair *) vp2)->window;

  Int4 result;                   /* result of the comparison */
  if(0 == (result = BLAST_CMP(w1->frame, w2->frame)) &&
     0 == (result = BLAST_CMP(w1->begin, w2->begin))) {
      result = BLAST_CMP(w1->end, w2->end);
  }
  return (int) result;
}


/**
 * Reads a array of HSPs and creates a new array of pointers to
 * Kappa_WindowInfo so that each element in the array of HSPs is
 * contained in exactly one window
 *
 * @param hsp_array         hsp array to be read [in]
 * @param hspcnt            length of hsp_array [in]
 * @param border            Number of extra amino acids to include
 *                          at the start and end of each HSP.
 * @param sequence_length   length of the sequence containing these
 *                          HSPs, in amino acid coordinates.
 * @param pwindows          a pointer to an array of windows;
 *                          the array may be resized by this routine. [in][out]
 * @param nWindows          the number of windows in *pwindows [in][out]
 * @param lWindows          the allocated length of *pwindows [in][out]
 * @param window_of_hsp     HSP i is contained in the bounds of
 *                          window_of_hsp[i]  [in][out]
 */
static void
WindowsFromHSPs(
  BlastHSP * hsp_array[],
  Int4 hspcnt,
  Int4 border,
  Int4 sequence_length,
  Kappa_WindowInfo ***pwindows,
  Int4 * nWindows,
  Int4 * lWindows,
  Int4 * window_of_hsp)
{
  Int4 k, ell;
  Kappa_WindowIndexPair * window_and_index;  /* an array of windows
                                              * paired with the index
                                              * of the HSP that
                                              * generated them */
  Kappa_WindowInfo     ** windows;      /* the output list of windows */
  Int4 start_cluster;    /* start of a cluster of windows to be joined */
  Int4 length_joined;    /* the current length of the list of joined windows */

  windows = *pwindows;
  /* Make the window list have exactly hspcnt windows. */
  if( *lWindows < hspcnt ) {
    *lWindows = 2 * hspcnt;
    windows = *pwindows =
      realloc(*pwindows, *lWindows *  sizeof(Kappa_WindowInfo*));
  }
  for( k = *nWindows; k < hspcnt; k++ ) {
    windows[k] = malloc(sizeof(Kappa_WindowInfo));
  }
  for( k = hspcnt; k < *nWindows; k++ ) {
    sfree(windows[k]);
  }
  *nWindows = hspcnt;

  window_and_index = calloc(hspcnt, sizeof(Kappa_WindowIndexPair));

  for( k = 0; k < hspcnt; k++ ) { /* for all HSPs */
    /* length of the translation of the nucleotide sequence in this frame */
    Int4 translated_length;

    windows[k]->frame = hsp_array[k]->subject.frame;

    if( windows[k]->frame > 0 ) {
      translated_length = (sequence_length - windows[k]->frame + 1)/3;
    } else {
      translated_length = (sequence_length + windows[k]->frame - 1)/3;
    }
    windows[k]->begin = MAX(0, hsp_array[k]->subject.offset - border);
    windows[k]->end   = MIN(translated_length,
                            hsp_array[k]->subject.end + border);
    windows[k]->hspcnt = 1;

    window_and_index[k].index  = k;
    window_and_index[k].window = windows[k];
  }
  qsort(window_and_index, hspcnt, sizeof(Kappa_WindowIndexPair),
        location_compare_windows);

  /* Join windows that overlap or are too close together.  */
  start_cluster = 0;
  length_joined = 0;
  for( k = 0; k < hspcnt; k++ ) {       /* for all windows in the
                                           original list */
    Kappa_WindowInfo * window;          /* window at this value of k */
    Kappa_WindowInfo * nextWindow;      /* window at the next value of k, or
                                           NULL if no such window exists */
    window     = window_and_index[k].window;
    nextWindow = ( k + 1 < hspcnt ) ? window_and_index[k+1].window : NULL;

    if(nextWindow != NULL                 && /* there is a next window; and */
       window->frame == nextWindow->frame && /* it is in the same frame; and
                                                it is very near this one */
       window->end + 3 * KAPPA_WINDOW_BORDER >= nextWindow->begin) {
      /* Join the current window with the next window.  Do not add the
         current window to the output list. */
      nextWindow->begin = MIN(window->begin, nextWindow->begin);
      nextWindow->end   = MAX(window->end,   nextWindow->end  );

      sfree(window);
      window_and_index[k].window = NULL;  /* Set the now dangling
                                             pointer to NULL */
    } else {
      /* Don't join the current window with the next window.  Add the
         current window to the output list instead */
      windows[length_joined] = window;
      for( ell = start_cluster; ell <= k; ell++ ) {
        window_of_hsp[window_and_index[ell].index] = length_joined;
      }
      length_joined++;
      start_cluster = k + 1;
    } /* end else don't join the current window with the next window */
  } /* end for all windows in the original list */
  *nWindows = length_joined;
  for( k = length_joined; k < hspcnt; k++ ) {
    windows[k] = NULL;
  }
  sfree(window_and_index);
}


/**
 * Redo a S-W alignment using an x-drop alignment.  The result will
 * usually be the same as the S-W alignment. The call to ALIGN
 * attempts to force the endpoints of the alignment to match the
 * optimal endpoints determined by the Smith-Waterman algorithm.
 * ALIGN is used, so that if the data structures for storing BLAST
 * alignments are changed, the code will not break
 *
 * @param query         the query data
 * @param queryStart    start of the alignment in the query sequence
 * @param queryEnd      end of the alignment in the query sequence,
 *                      as computed by the Smith-Waterman algorithm
 * @param subject       the subject (database) sequence
 * @param matchStart    start of the alignment in the subject sequence
 * @param matchEnd      end of the alignment in the query sequence,
 *                      as computed by the Smith-Waterman algorithm
 * @param scoringParams Settings for gapped alignment.[in]
 * @param gap_align     parameters for a gapped alignment
 * @param score         score computed by the Smith-Waterman algorithm
 * @param localScalingFactor    the factor by which the scoring system has
 *                              been scaled in order to obtain greater
 *                              precision
 * @param queryAlignmentExtent  length of the alignment in the query sequence,
 *                              as computed by the x-drop algorithm
 * @param matchAlignmentExtent  length of the alignment in the subject
 *                              sequence, as computed by the x-drop algorithm
 * @param newScore              alignment score computed by the x-drop
 *                              algorithm
 */
static void
Kappa_SWFindFinalEndsUsingXdrop(
  Kappa_SequenceData * query,
  Int4 queryStart,
  Int4 queryEnd,
  Kappa_SequenceData * subject,
  Int4 matchStart,
  Int4 matchEnd,
  BlastGapAlignStruct* gap_align,
  const BlastScoringParameters* scoringParams,
  Int4 score,
  double localScalingFactor,
  Int4 * queryAlignmentExtent,
  Int4 * matchAlignmentExtent,
  Int4 * newScore)
{
  Int4 XdropAlignScore;         /* alignment score obtained using X-dropoff
                                 * method rather than Smith-Waterman */
  Int4 doublingCount = 0;       /* number of times X-dropoff had to be
                                 * doubled */

  GapPrelimEditBlockReset(gap_align->rev_prelim_tback);
  GapPrelimEditBlockReset(gap_align->fwd_prelim_tback);
  do {
    XdropAlignScore =
      ALIGN_EX(&(query->data[queryStart]) - 1,
               &(subject->data[matchStart]) - 1,
               queryEnd - queryStart + 1, matchEnd - matchStart + 1,
               queryAlignmentExtent,
               matchAlignmentExtent, gap_align->fwd_prelim_tback,
               gap_align, scoringParams, queryStart - 1, FALSE, FALSE);

    gap_align->gap_x_dropoff *= 2;
    doublingCount++;
    if((XdropAlignScore < score) && (doublingCount < 3)) {
      GapPrelimEditBlockReset(gap_align->fwd_prelim_tback);
    }
  } while((XdropAlignScore < score) && (doublingCount < 3));

  *newScore = XdropAlignScore;
}


/**
 * A Kappa_MatchingSequence represents a subject sequence to be aligned
 * with the query.  This abstract sequence is used to hide the
 * complexity associated with actually obtaining and releasing the
 * data for a matching sequence, e.g. reading the sequence from a DB
 * or translating it from a nucleotide sequence.
 *
 * We draw a distinction between a sequence itself, and strings of
 * data that may be obtained from the sequence.  The amino
 * acid/nucleotide data is represented by an object of type
 * Kappa_SequenceData.  There may be more than one instance of
 * Kappa_SequenceData per Kappa_MatchingSequence, each representing a
 * different range in the sequence, or a different translation frame.
 */
typedef struct Kappa_MatchingSequence {
  Int4          length;         /**< length of this matching sequence */
  Int4          index;          /**< index of this sequence in the database */
  EBlastProgramType prog_number; /**< identifies the type of blast search being
                                     performed. The type of search determines
                                     how sequence data should be obtained. */
  const Uint1*   genetic_code;   /**< genetic code for translated searches */
  const BlastSeqSrc* seq_src;   /**< BLAST sequence data source */
  BlastSeqSrcGetSeqArg seq_arg;            /**< argument to GetSequence method of the
                                  BlastSeqSrc (@todo this structure was
                                  designed to be allocated on the stack, i.e.:
                                  in Kappa_MatchingSequenceInitialize)
                                 */
} Kappa_MatchingSequence;


/**
 * Initialize a new matching sequence, obtaining information about the
 * sequence from the search.
 *
 * @param self              object to be initialized
 * @param seqSrc            A pointer to a source from which sequence data
 *                          may be obtained
 * @param program_number    identifies the type of blast search being
                            performed.
 * @param gen_code_string   genetic code for translated queries
 * @param subject_index     index of the matching sequence in the database
 */
static void
Kappa_MatchingSequenceInitialize(
  Kappa_MatchingSequence * self,
  EBlastProgramType program_number,
  const BlastSeqSrc* seqSrc,
  const Uint1* gen_code_string,
  Int4 subject_index)
{
  self->seq_src      = seqSrc;
  self->prog_number  = program_number;
  self->genetic_code = gen_code_string;

  memset((void*) &self->seq_arg, 0, sizeof(self->seq_arg));
  self->seq_arg.oid = self->index = subject_index;

  if( program_number == eBlastTypeTblastn ) {
    self->seq_arg.encoding = NCBI4NA_ENCODING;
  } else {
    self->seq_arg.encoding = BLASTP_ENCODING;
  }

  if (BlastSeqSrcGetSequence(seqSrc, (void*) &self->seq_arg) < 0)
    return;
  self->length = BlastSeqSrcGetSeqLen(seqSrc, (void*) &self->seq_arg);
}


/** Release the resources associated with a matching sequence. */
static void
Kappa_MatchingSequenceRelease(Kappa_MatchingSequence * self)
{
  BlastSeqSrcReleaseSequence(self->seq_src, (void*)&self->seq_arg);
  BlastSequenceBlkFree(self->seq_arg.seq);
}


/** NCBIstdaa encoding for 'X' character (@todo is this really needed?) */
#define BLASTP_MASK_RESIDUE 21
/** Default instructions and mask residue for SEG filtering */
#define BLASTP_MASK_INSTRUCTIONS "S 10 1.8 2.1"


/**
 * Obtain a string of translated data
 *
 * @param self          the sequence from which to obtain the data [in]
 * @param window        the range and tranlation frame to get [in]
 * @param seqData       the resulting data [out]
 */
static void
Kappa_SequenceGetTranslatedWindow(Kappa_MatchingSequence * self,
                                  Kappa_WindowInfo * window,
                                  Kappa_SequenceData * seqData )
{
  ASSERT( 0 && "Not implemented" );
}


/**
 * Obtain the sequence data that lies within the given window.
 *
 * @param self          sequence information [in]
 * @param window        window specifying the range of data [in]
 * @param seqData       the sequence data obtained [out]
 */
static void
Kappa_SequenceGetWindow(
  Kappa_MatchingSequence * self,
  Kappa_WindowInfo * window,
  Kappa_SequenceData * seqData )
{
  if(self->prog_number ==  eBlastTypeTblastn) {
    /* The sequence must be translated. */
    Kappa_SequenceGetTranslatedWindow(self, window, seqData);
  } else {
    /* The sequence does not need to be translated. */
    /* Copy the entire sequence (necessary for SEG filtering.) */
    seqData->buffer  = calloc((self->length + 2), sizeof(Uint1));
    seqData->data    = seqData->buffer + 1;
    seqData->length  = self->length;

    memcpy(seqData->data, self->seq_arg.seq->sequence, seqData->length);
#ifndef KAPPA_NO_SEG_SEQUENCE
    /*take as input an amino acid  string and its length; compute a filtered
      amino acid string and return the filtered string*/
    {{
      BlastSeqLoc* mask_seqloc;
      const EBlastProgramType k_program_name = eBlastTypeBlastp;
      SBlastFilterOptions* filter_options;

      BlastFilteringOptionsFromString(k_program_name, BLASTP_MASK_INSTRUCTIONS, &filter_options, NULL);

      BlastSetUp_Filter(k_program_name, seqData->data, seqData->length,
                        0, filter_options, &mask_seqloc, NULL);

      filter_options = SBlastFilterOptionsFree(filter_options);

      Blast_MaskTheResidues(seqData->data, seqData->length,
                            FALSE, mask_seqloc, FALSE, 0);

      mask_seqloc = BlastSeqLocFree(mask_seqloc);
    }}
#endif
    /* Fit the data to the window. */
    seqData ->data    = &seqData->data[window->begin - 1];
    *seqData->data++  = '\0';
    seqData ->length  = window->end - window->begin;
  } /* end else the sequence does not need to be translated */
}


/**
 * Computes an appropriate starting point for computing the traceback
 * for an HSP.  The start point depends on the matrix, the window, and
 * the filtered sequence, and so may not be the start point saved in
 * the HSP during the preliminary gapped extension.
 *
 * @param q_start       the start point in the query [out]
 * @param s_start       the start point in the subject [out]
 * @param sbp           general scoring info (includes the matrix) [in]
 * @param positionBased is this search position-specific? [in]
 * @param hsp           the HSP to be considered [in]
 * @param window        the window used to compute the traceback [in]
 * @param query         the query data [in]
 * @param subject       the subject data [in]
 */
static void
StartingPointForHit(
  Int4 * q_start,
  Int4 * s_start,
  const BlastScoreBlk* sbp,
  Boolean positionBased,
  BlastHSP * hsp,
  Kappa_WindowInfo * window,
  Kappa_SequenceData * query,
  Kappa_SequenceData * subject)
{
  hsp->subject.offset       -= window->begin;
  hsp->subject.gapped_start -= window->begin;

  if(BLAST_CheckStartForGappedAlignment(hsp, query->data,
                                         subject->data, sbp)) {
    /* We may use the starting point supplied by the HSP. */
    *q_start = hsp->query.gapped_start;
    *s_start = hsp->subject.gapped_start;
  } else {
    /* We must recompute the start for the gapped alignment, as the
       one in the HSP was unacceptable.*/
    *q_start =
      BlastGetStartForGappedAlignment(query->data, subject->data, sbp,
          hsp->query.offset, hsp->query.end - hsp->query.offset,
          hsp->subject.offset, hsp->subject.end - hsp->subject.offset);

    *s_start =
      (hsp->subject.offset - hsp->query.offset) + *q_start;
  }
}


/**
 * Create a new Kappa_DistinctAlignment and append the list of
 * alignments represented by "next."
 *
 * @param query         query sequence data
 * @param queryStart    the start of the alignment in the query
 * @param queryEnd      the end of the alignment in the query
 * @param subject       subject sequence data
 * @param matchStart    the start of the alignment in the subject window
 * @param matchEnd      the end of the alignment in the subject window
 * @param score         the score of this alignment
 * @param window        the subject window of this alignment
 * @param gap_align     alignment info for gapped alignments
 * @param scoringParams Settings for gapped alignment.[in]
 * @param localScalingFactor    the factor by which the scoring system has
 *                              been scaled in order to obtain greater
 *                              precision
 * @param prog_number   the type of alignment being performed
 * @param next          preexisting list of alignments [out]
 */
static Kappa_DistinctAlignment *
NewAlignmentUsingXdrop(
  Kappa_SequenceData * query,
  Int4 queryStart,
  Int4 queryEnd,
  Kappa_SequenceData * subject,
  Int4 matchStart,
  Int4 matchEnd,
  Int4 score,
  Kappa_WindowInfo * window,
  BlastGapAlignStruct * gap_align,
  const BlastScoringParameters* scoringParams,
  double localScalingFactor,
  Int4 prog_number,
  Kappa_DistinctAlignment * next)
{
  Int4 newScore;
  /* Extent of the alignment as computed by an x-drop alignment
   * (usually the same as (queryEnd - queryStart) and (matchEnd -
   * matchStart)) */
  Int4 queryExtent, matchExtent;
  Kappa_DistinctAlignment * obj;  /* the new object */

  Kappa_SWFindFinalEndsUsingXdrop(query,   queryStart, queryEnd,
                                  subject, matchStart, matchEnd,
                                  gap_align, scoringParams,
                                  score, localScalingFactor,
                                  &queryExtent, &matchExtent,
                                  &newScore);
  obj = malloc(sizeof(Kappa_DistinctAlignment));
  obj->editScript = 
      Blast_PrelimEditBlockToGapEditScript(gap_align->rev_prelim_tback,
                                           gap_align->fwd_prelim_tback);

  obj->score      = newScore;
  obj->queryStart = queryStart;
  obj->queryEnd   = obj->queryStart + queryExtent;
  obj->matchStart = matchStart      + window->begin;
  obj->matchEnd   = obj->matchStart + matchExtent;
  obj->frame      = window->frame;

  obj->next       = next;

  return obj;
}


/**
 * Reads a GapAlignBlk that has been used to compute a traceback, and
 * return a Kappa_DistinctAlignment representing the alignment.
 *
 * @param gap_align         the GapAlignBlk
 * @param window            the window used to compute the traceback
 */
static Kappa_DistinctAlignment *
NewAlignmentFromGapAlign(
  BlastGapAlignStruct * gap_align,
  Kappa_WindowInfo * window)
{
  Kappa_DistinctAlignment * obj; /* the new alignment */
  obj = malloc(sizeof(Kappa_DistinctAlignment));

  obj->score      = gap_align->score;
  obj->queryStart = gap_align->query_start;
  obj->queryEnd   = gap_align->query_stop;
  obj->matchStart = gap_align->subject_start + window->begin;
  obj->matchEnd   = gap_align->subject_stop  + window->begin;
  obj->frame      = window->frame;

  obj->editScript        = gap_align->edit_script;
  gap_align->edit_script = NULL; /* set to NULL to avoid aliasing */
  obj->next             = NULL;

  return obj;
}


/**
 * A Kappa_SearchParameters represents the data needed by
 * RedoAlignmentCore to adjust the parameters of a search, including
 * the original value of these parameters
 */
typedef struct Kappa_SearchParameters {
  Int4          gapOpen;        /**< a penalty for the existence of a gap */
  Int4          gapExtend;      /**< a penalty for each residue (or
                                      nucleotide) in the gap */
  Int4          gapDecline;     /**< a penalty for declining to align a pair
                                     of residues */
  Int4          mRows;          /**< the number of rows in a scoring matrix. */
  Int4          nCols;          /**< the number of columns in a scoring
                                     matrix */

  double   scaledUngappedLambda;   /**< The value of Karlin-Altschul
                                             parameter lambda, rescaled
                                             to allow scores to have
                                             greater precision */
  Int4 **origMatrix;            /**< The original matrix values */
  Int4 **startMatrix;           /**< Rescaled values of the original matrix */

  double **startFreqRatios;             /**< frequency ratios to start
                                             investigating each pair */
  double  *scoreArray;          /**< array of score probabilities */
  double  *resProb;             /**< array of probabilities for each residue
                                     in a matching sequence */
  double  *queryProb;           /**< array of probabilities for each residue
                                     in the query */
  Boolean       adjustParameters;       /**< Use composition-based statistics
                                             if true. */

  Blast_ScoreFreq* return_sfp;          /**< score frequency pointers to
                                             compute lambda */
  Blast_KarlinBlk *kbp_gap_orig;    /**< copy of the original gapped
                                      Karlin-Altschul block corresponding to
                                      the first context */
  Blast_KarlinBlk **orig_kbp_gap_array; /**< pointer to the array of gapped
                                          Karlin-Altschul block for all
                                          contexts (@todo is this really
                                          needed?) */
  double scale_factor;      /**< The original scale factor (to be restored). */
} Kappa_SearchParameters;


/**
 * Release the data associated with a Kappa_SearchParameters and
 * delete the object
 * @param searchParams the object to be deleted [in][out]
 */
static void
Kappa_SearchParametersFree(Kappa_SearchParameters ** searchParams)
{
  /* for convenience, remove one level of indirection from searchParams */
  Kappa_SearchParameters *sp = *searchParams;

  if(sp->kbp_gap_orig) Blast_KarlinBlkFree(sp->kbp_gap_orig);

  if(sp->startMatrix)
    _PSIDeallocateMatrix((void**) sp->startMatrix, sp->mRows);
  if(sp->origMatrix)
    _PSIDeallocateMatrix((void**) sp->origMatrix, sp->mRows);
  if(sp->startFreqRatios) 
    _PSIDeallocateMatrix((void**) sp->startFreqRatios, sp->mRows);

  if(sp->return_sfp) sfree(sp->return_sfp);
  if(sp->scoreArray) sfree(sp->scoreArray);
  if(sp->resProb)    sfree(sp->resProb);
  if(sp->queryProb)  sfree(sp->queryProb);

  sfree(*searchParams);
  *searchParams = NULL;
}


/**
 * Create a new instance of Kappa_SearchParameters
 *
 * @param rows              number of rows in the scoring matrix
 * @param adjustParameters  if true, use composition-based statistics
 * @param positionBased     if true, the search is position-based
 */
static Kappa_SearchParameters *
Kappa_SearchParametersNew(
  Int4 rows,
  Boolean adjustParameters,
  Boolean positionBased)
{
  Kappa_SearchParameters *sp;   /* the new object */
  sp = malloc(sizeof(Kappa_SearchParameters));

  sp->orig_kbp_gap_array = NULL;
  
  sp->mRows = positionBased ? rows : BLASTAA_SIZE;
  sp->nCols = BLASTAA_SIZE;
    
  sp->kbp_gap_orig     = NULL;
  sp->startMatrix      = NULL;
  sp->origMatrix       = NULL;
  sp->startFreqRatios  = NULL;
  sp->return_sfp       = NULL;
  sp->scoreArray       = NULL;
  sp->resProb          = NULL;
  sp->queryProb        = NULL;
  sp->adjustParameters = adjustParameters;
  
  if(adjustParameters) {
    sp->kbp_gap_orig = Blast_KarlinBlkNew();
    sp->startMatrix  = (Int4**) _PSIAllocateMatrix(sp->mRows, sp->nCols,
                                                   sizeof(Int4));
    sp->origMatrix   = (Int4**) _PSIAllocateMatrix(sp->mRows, sp->nCols,
                                                   sizeof(Int4));
    sp->resProb    =
      (double *) calloc(BLASTAA_SIZE, sizeof(double));
    sp->scoreArray =
      (double *) calloc(kScoreMatrixScoreRange, sizeof(double));
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


/**
 * Record the initial value of the search parameters that are to be
 * adjusted.
 *
 * @param searchParams the object to be filled in [in|out]
 * @param queryBlk query sequence [in]
 * @param queryInfo query sequence information [in]
 * @param sbp Scoring Blk (contains Karlin-Altschul parameters) [in]
 * @param scoring gap-open/extend/decline_align information [in]
 * @param positionBased is this search position-specific? [in]
 * @todo instead of hard coding 0 for context we should use queryInfo
 */
static void
Kappa_RecordInitialSearch(Kappa_SearchParameters * searchParams,
                          BLAST_SequenceBlk * queryBlk,
                          BlastQueryInfo* queryInfo,
                          BlastScoreBlk* sbp,
                          const BlastScoringParameters* scoring,
                          Boolean positionBased)
{
  Uint1* query;               /* the query sequence */
  Int4 queryLength;             /* the length of the query sequence */
  const Int4 kContextOffset = queryInfo->contexts[0].query_offset;  /* offset in buffer of start of query. */

  query = &queryBlk->sequence[kContextOffset];
  queryLength = queryInfo->contexts[0].query_length;
  ASSERT((0 == queryInfo->first_context) &&
         (queryInfo->first_context == queryInfo->last_context));
  
  if(searchParams->adjustParameters) {
    Int4 i, j;
    Blast_KarlinBlk* kbp;     /* statistical parameters used to evaluate a
                                 * query-subject pair */
    Int4 **matrix;       /* matrix used to score a local
                                   query-subject alignment */

    if(positionBased) {
      matrix = sbp->psi_matrix->pssm->data;
      ASSERT(queryLength == searchParams->mRows);
      ASSERT(queryLength == (Int4)sbp->psi_matrix->pssm->ncols);
    } else {
      matrix = sbp->matrix->data;
      Blast_FillResidueProbability(query, queryLength, searchParams->queryProb);
    }
    kbp = sbp->kbp_gap[0];
    searchParams->gapOpen    = scoring->gap_open;
    searchParams->gapExtend  = scoring->gap_extend;
    searchParams->gapDecline = scoring->decline_align;
    searchParams->scale_factor   = scoring->scale_factor;

    searchParams->orig_kbp_gap_array   = sbp->kbp_gap;

    Blast_KarlinBlkCopy(searchParams->kbp_gap_orig, kbp);

    for(i = 0; i < searchParams->mRows; i++) {
      for(j = 0; j < BLASTAA_SIZE; j++) {
        searchParams->origMatrix[i][j] = matrix[i][j];
      }
    }
  }
}


/**
 * Rescale the search parameters in the search object and options
 * object to obtain more precision.
 *
 * @param sp record of parameters used and frequencies [in|out]
 * @param queryBlk query sequence [in]
 * @param queryInfo query sequence information [in]
 * @param sbp Scoring Blk (contains Karlin-Altschul parameters) [in]
 * @param scoringParams gap-open/extend/decline_align information [in]
 * @param positionBased is this search position-specific? [in]
 * @return scaling-factor to be used.
 */
static double
Kappa_RescaleSearch(Kappa_SearchParameters * sp,
                    BLAST_SequenceBlk* queryBlk,
                    BlastQueryInfo* queryInfo,
                    BlastScoreBlk* sbp,
                    BlastScoringParameters* scoringParams,
                    Boolean positionBased)
{
  double localScalingFactor;            /* the factor by which to
                                         * scale the scoring system in
                                         * order to obtain greater
                                         * precision */

  if(!sp->adjustParameters) {
    localScalingFactor = 1.0;
  } else {
    double initialUngappedLambda;       /* initial value of the
                                         * statistical parameter
                                         * lambda used to evaluate
                                         * ungapped alignments */
    Blast_KarlinBlk* kbp;       /* the statistical parameters used to
                                 * evaluate alignments of a
                                 * query-subject pair */
    Uint1* query;               /* the query sequence */
    Int4 queryLength;           /* the length of the query sequence */

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
    queryLength = queryInfo->contexts[0].query_length;
    if(positionBased) {
      int status = 0;
      ASSERT(queryLength == sp->mRows);
      ASSERT(queryLength == (Int4)sbp->psi_matrix->pssm->ncols);
      sp->startFreqRatios =
        getStartFreqRatios(sbp, query, scoringParams->options->matrix,
                           sbp->psi_matrix->freq_ratios, queryLength);
      status = scalePosMatrix(sp->startMatrix, sbp->matrix->data, 
                              scoringParams->options->matrix, 
                              sbp->psi_matrix->freq_ratios, query, 
                              queryLength, sbp);
      if (status) {
          return 0.0;   /* return incorrect value for scalingFactor */
      }
      initialUngappedLambda = sbp->kbp_psi[0]->Lambda;
    } else {
      SFreqRatios* freqRatios =
          _PSIMatrixFrequencyRatiosNew(scoringParams->options->matrix);
      sp->startFreqRatios = (double**) _PSIAllocateMatrix(sp->mRows,
                                                          sp->nCols,
                                                          sizeof(double));
      ASSERT(sp->startFreqRatios);
      _PSICopyMatrix_double(sp->startFreqRatios, freqRatios->data,
                            sp->mRows, sp->nCols);
      freqRatios = _PSIMatrixFrequencyRatiosFree(freqRatios);
      initialUngappedLambda = sbp->kbp_ideal->Lambda;
    }
    sp->scaledUngappedLambda = initialUngappedLambda / localScalingFactor;
    if(!positionBased) {
      computeScaledStandardMatrix(sp->startMatrix,
                                  scoringParams->options->matrix,
                                  sp->scaledUngappedLambda);
    }
    kbp = sbp->kbp_gap[0];
    kbp->Lambda /= localScalingFactor;
    kbp->logK = log(kbp->K);
  }

  return localScalingFactor;
}

/** LambdaRatioLowerBound is used when the expected score is too large
 * causing impalaKarlinLambdaNR to give a Lambda estimate that
 * is too small, or to fail entirely returning -1 */
#define LambdaRatioLowerBound 0.5

/**
 * Adjust the search parameters
 *
 * @param sp            a record of the initial search parameters [in|out]
 * @param queryLength   length of query sequence [in]
 * @param subject       data from the subject sequence [in]
 * @param matrix        a scoring matrix to be adjusted [out]
 * @param positionBased is this search position-specific? [in]
 * @return              scaling-factor to be used.
 */
static Int4
Kappa_AdjustSearch(
  Kappa_SearchParameters * sp,
  Int4 queryLength,
  Kappa_SequenceData * subject,
  Int4 ** matrix,
  Boolean positionBased)
{
  double LambdaRatio;           /* the ratio of the corrected lambda to the
                                 * original lambda */
  if(!sp->adjustParameters) {
    LambdaRatio = 1.0;
  } else {
    /* do adjust the parameters */
    Blast_ScoreFreq* this_sfp; 
    double correctUngappedLambda;  /* new value of ungapped lambda */

    /* compute and plug in new matrix here */
    Blast_FillResidueProbability(subject->data, subject->length, sp->resProb);

    if(positionBased) {
      ASSERT(queryLength == sp->mRows);
      this_sfp =
        posfillSfp(sp->startMatrix, queryLength, sp->resProb, sp->scoreArray,
                   sp->return_sfp, kScoreMatrixScoreRange);
    } else {
      this_sfp =
        notposfillSfp(sp->startMatrix, sp->resProb, sp->queryProb,
                      sp->scoreArray, sp->return_sfp, kScoreMatrixScoreRange);
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


/**
 * Restore the parameters that were adjusted to their original values
 * @param searchParams      a record of the original values [in]
 * @param sbp               Karlin-Altschul parameters to be restored. [out]
 * @param matrix            the scoring matrix to be restored [out]
 * @param scoring           the scoring parameters to be restored [out]
 * @param positionBased     is this search position-specific? [in]
 */
static void
Kappa_RestoreSearch(
  Kappa_SearchParameters * searchParams,
  BlastScoreBlk* sbp,
  Int4 ** matrix,
  BlastScoringParameters* scoring,
  Boolean positionBased)
{
  if(searchParams->adjustParameters) {
    Blast_KarlinBlk* kbp;       /* statistical parameters used to
                                   evaluate the significance of
                                   alignment of a query-subject
                                   pair */
    Int4 i, j; /* loop variables. */

    scoring->gap_open = searchParams->gapOpen;
    scoring->gap_extend = searchParams->gapExtend;
    scoring->decline_align = searchParams->gapDecline;
    scoring->scale_factor = searchParams->scale_factor;

    sbp->kbp_gap       = searchParams->orig_kbp_gap_array;

    kbp = sbp->kbp_gap[0];
    Blast_KarlinBlkCopy(kbp, searchParams->kbp_gap_orig);

    for(i = 0; i < searchParams->mRows; i++) {
      for(j = 0; j < BLASTAA_SIZE; j++) {
        matrix[i][j] = searchParams->origMatrix[i][j];
      }
    }
  }
}

Int2
Kappa_RedoAlignmentCore(EBlastProgramType program_number,
                  BLAST_SequenceBlk * queryBlk,
                  BlastQueryInfo* queryInfo,
                  BlastScoreBlk* sbp,
                  BlastHSPStream* hsp_stream,
                  const BlastSeqSrc* seqSrc,
                  const Uint1* gen_code_string,
                  BlastScoringParameters* scoringParams,
                  const BlastExtensionParameters* extendParams,
                  const BlastHitSavingParameters* hitParams,
                  const PSIBlastOptions* psiOptions,
                  BlastHSPResults* results)
{
  Int4 cutoff_s = 0;            /* minimum score that must be achieved
                                   by a newly-computed alignment */
  Boolean do_link_hsps;         /* if true, use BlastLinkHsps to
                                   compute e-values */
  Kappa_SequenceData query;     /* data for the query sequence */
  double localScalingFactor;       /* the factor by which to
                                         * scale the scoring system in
                                         * order to obtain greater
                                         * precision */

  Int4** matrix = NULL;         /* score matrix */
  Blast_KarlinBlk* kbp;         /* stores Karlin-Altschul parameters */
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
  Kappa_WindowInfo ** windows; /* windows containing HSPs for
                                * a single query-subject pair */
  Int4 nWindows;               /* number of windows in the array
                                * "windows" */
  Int4 lWindows;               /* allocated size of "windows" */
  Int4 window_index;           /* window index for use in loops */
  int status_code;             /* status code of any routine that
                                  returns one */

  BlastGapAlignStruct* gapAlign;        /* keeps track of gapped
                                           alignment params */
  Boolean SmithWaterman;       /* Perform Smith-Waterman alignments? */
  /* is this search position-specific? */
  Boolean positionBased = (sbp->psi_matrix ? TRUE : FALSE); 
  Boolean adjustParameters;    /* Use composition based statistics? */
  BlastHSPList* thisMatch = NULL;  /* alignment data for the
                                    * current query-subject
                                    * match */

  double inclusion_ethresh;    /* All alignments above this value will be
                                  reported, no matter how many. */

  if (program_number != eBlastTypeBlastp &&
      program_number != eBlastTypePsiBlast &&
      program_number != eBlastTypePhiBlastp) { /* tblastn ported but not fully
                                                 implemented */
        return BLASTERR_REDOALIGNMENTCORE_NOTSUPPORTED;
  }

  inclusion_ethresh =
    (psiOptions != NULL) ? psiOptions->inclusion_ethresh : 0;

  adjustParameters = extendParams->options->compositionBasedStats;

  if (extendParams->options->eTbackExt ==  eSmithWatermanTbck)
    SmithWaterman = TRUE;
  else
    SmithWaterman = FALSE;

  if ((status_code=BLAST_GapAlignStructNew(scoringParams, extendParams,
                 BlastSeqSrcGetMaxSeqLen(seqSrc), sbp, &gapAlign)) != 0)
      return status_code;

  /* Initialize the window list to have a single window -- the most
     common case */
  lWindows   = 1;   nWindows = 1;
  windows    = calloc(lWindows, sizeof(Kappa_WindowInfo *));
  windows[0] = malloc(sizeof(Kappa_WindowInfo));

  SWheapInitialize(&significantMatches, hitParams->options->hitlist_size,
                   hitParams->options->hitlist_size, inclusion_ethresh);

  /**** Validate parameters *************/
  if(0 == strcmp(scoringParams->options->matrix, "BLOSUM62_20") &&
     !adjustParameters) {
    return 0;                   /* BLOSUM62_20 only makes sense if
                                 * adjustParameters is on */
  }
  /*****************/
  query.data   = &queryBlk->sequence[0];
  query.length = queryInfo->contexts[0].query_length;

  if(SmithWaterman) {
    Kappa_ForbiddenRangesInitialize(&forbidden, query.length);
  }

  if(positionBased) {
    ASSERT(program_number == eBlastTypePsiBlast);
    matrix = sbp->psi_matrix->pssm->data;
    ASSERT( matrix != NULL );

    if(sbp->psi_matrix->freq_ratios == NULL) {
      sbp->psi_matrix->freq_ratios =
        (double**) _PSIAllocateMatrix(query.length, BLASTAA_SIZE,
                                      sizeof(double));
    }
  } else {
    matrix = sbp->matrix->data;
  }
  kbp = sbp->kbp_gap[0];

  /* Initialize searchParams */
  searchParams =
    Kappa_SearchParametersNew(query.length, adjustParameters, positionBased);
  Kappa_RecordInitialSearch(searchParams, queryBlk, queryInfo, sbp,
                            scoringParams, positionBased);
  localScalingFactor = Kappa_RescaleSearch(searchParams, queryBlk, queryInfo,
                                           sbp, scoringParams, positionBased);
  ASSERT(localScalingFactor != 0.0);


  do_link_hsps = program_number == eBlastTypeTblastn;
  if(do_link_hsps) {
    ASSERT( 0 && "Which cutoff needed here?" );
    /*     cutoff_s = search->pbp->cutoff_s2 * localScalingFactor; */
  } else {
    /* There is no cutoff score; we consider e-values instead */
    cutoff_s = 0;
  }
  while (BlastHSPStreamRead(hsp_stream, &thisMatch) != kBlastHSPStream_Eof) {
    /* for all matching sequences */
    Kappa_MatchingSequence matchingSeq; /* the data for a matching
                                         * database sequence */
    Int4 * window_of_hsp;               /* index of each HSP in the
                                         * array "windows" */
    Kappa_WindowInfo * window;          /* current window in the
                                         * subject sequence */
    Kappa_DistinctAlignment * alignments;   /* list of alignments for this
                                             * query-subject pair */
    alignments = NULL;

    if(thisMatch->hsp_array == NULL) {
      continue;
    }

    if(SWheapWillAcceptOnlyBelowCutoff(&significantMatches)) {
      /* Only matches with evalue <= options->ethresh will be saved */

      /* e-value for a sequence is the smallest e-value among the HSPs
       * matching a region of the sequence to the query */
      double minEvalue = thisMatch->best_evalue;
      if(minEvalue > (EVALUE_STRETCH * inclusion_ethresh)) {
        /* This match is likely to have an evalue > options->ethresh
         * and therefore, we assume that all other matches with higher
         * input e-values are also unlikely to get sufficient
         * improvement in a redone alignment */
        break;
      }
    }
    /* Get the sequence for this match */
    Kappa_MatchingSequenceInitialize(&matchingSeq, program_number,
                                     seqSrc, gen_code_string, thisMatch->oid);

    window_of_hsp = calloc(thisMatch->hspcnt, sizeof(Int4));
    if(program_number == eBlastTypeTblastn) {
      /* Find the multiple translation windows used by tblastn queries. */
      WindowsFromHSPs(thisMatch->hsp_array, thisMatch->hspcnt,
                      KAPPA_WINDOW_BORDER, matchingSeq.length,
                      &windows, &nWindows, &lWindows, window_of_hsp);
    } else { /* the program is not tblastn, i.e. it is blastp */
      /* Initialize the single window used by blastp queries. */
      windows[0]->frame  = 0;
      windows[0]->hspcnt = thisMatch->hspcnt;
      windows[0]->begin  = 0;
      windows[0]->end    = matchingSeq.length;
    } /* else the program is blastp */
    if(SmithWaterman) {
      /* We are performing a Smith-Waterman alignment */
      for(window_index = 0; window_index < nWindows; window_index++) {
        /* for all window */
        Kappa_SequenceData subject;     /* sequence data for this window */

        window = windows[window_index];
        Kappa_SequenceGetWindow( &matchingSeq, window, &subject );

        if(0 ==
           Kappa_AdjustSearch(searchParams, query.length, &subject, matrix,
                              positionBased)) {
          /* Kappa_AdjustSearch ran without error; compute the new
             alignments. */
          Int4 aSwScore;                    /* score computed by the
                                             * Smith-Waterman algorithm. */
          Boolean alignment_is_significant; /* True if the score/evalue of
                                             * the Smith-Waterman alignment
                                             * is significant. */
          Kappa_ForbiddenRangesClear(&forbidden);
          do {
            double newSwEvalue;         /* evalue as computed by the
                                         * Smith-Waterman algorithm */
            Int4 matchEnd, queryEnd;    /* end points of the alignments
                                         * computed by the Smith-Waterman
                                         * algorithm. */
            newSwEvalue =
              SmithWatermanScoreOnly(&subject, &query, matrix,
                                     scoringParams->gap_open,
                                     scoringParams->gap_extend,
                                     &matchEnd, &queryEnd, &aSwScore, kbp,
                                     queryInfo->contexts[0].eff_searchsp,
                                     positionBased,
                                     &forbidden);
            alignment_is_significant =
              ( do_link_hsps && aSwScore >= cutoff_s) ||
              (!do_link_hsps && newSwEvalue <
               hitParams->options->expect_value &&
               SWheapWouldInsert(&significantMatches, newSwEvalue));

            if(alignment_is_significant) {
              Int4 matchStart, queryStart;  /* the start of the
                                             * alignment in the
                                             * match/query sequence */

              SmithWatermanFindStart(&subject, &query, matrix,
                                     scoringParams->gap_open,
                                     scoringParams->gap_extend,
                                     matchEnd, queryEnd, aSwScore,
                                     &matchStart, &queryStart,
                                     positionBased, &forbidden);

              gapAlign->gap_x_dropoff =
                (Int4) (extendParams->gap_x_dropoff_final *
                        NCBIMATH_LN2 / kbp->Lambda);

              alignments =
                NewAlignmentUsingXdrop(&query,   queryStart, queryEnd,
                                       &subject, matchStart, matchEnd,
                                       aSwScore, window,
                                       gapAlign, scoringParams,
                                       localScalingFactor,
                                       program_number, alignments);

              Kappa_ForbiddenRangesPush(&forbidden,
                                        queryStart,
                                        alignments->queryEnd - queryStart,
                                        matchStart,
                                        alignments->matchEnd - matchStart);
            }
            /* end if the next local alignment is significant */
          } while(alignment_is_significant && window->hspcnt > 1);
          /* end do..while the next local alignment is significant, and
           * the original blast search found more than one alignment. */
        } /* end if Kappa_AdjustSearch ran without error.  */
        Kappa_SequenceDataRelease(&subject);
      } /* end for all windows */
    } else {
      /* else we are not performing a Smith-Waterman alignment */
      Int4 hsp_index;
      /* data for the current window */
      Kappa_SequenceData subject = {NULL,0,NULL};
      window_index  = -1;       /* -1 indicates that sequence data has
                                 * not been obtained for any window in
                                 * the list. */
      window        = NULL;

      for(hsp_index = 0; hsp_index < thisMatch->hspcnt; hsp_index++) {
        /* for all HSPs in thisMatch */
        if(!isAlreadyContained(thisMatch->hsp_array[hsp_index], alignments,
                               kbp->Lambda, localScalingFactor)) {
          Kappa_DistinctAlignment * newAlign;   /* the new alignment */
          Boolean adjust_search_failed = FALSE; /* if true, AdjustSearch was
                                                 * called and failed. */
          if( window_index != window_of_hsp[hsp_index] ) {
            /* The current window doesn't contain this HSP. */
            Kappa_SequenceDataRelease(&subject);

            window_index = window_of_hsp[hsp_index];
            window       = windows[window_index];
            Kappa_SequenceGetWindow(&matchingSeq, window, &subject);

            adjust_search_failed =
              Kappa_AdjustSearch(searchParams, query.length, &subject, matrix,
                                 positionBased);
          }  /* end if the current window doesn't contain this HSP */
          if(!adjust_search_failed) {
            Int4 q_start, s_start;

            StartingPointForHit(&q_start, &s_start, sbp, positionBased,
                                thisMatch->hsp_array[hsp_index],
                                window, &query, &subject);

            if (positionBased) {
                /* We don't use the scaled Lambda because we loose precision */
                gapAlign->gap_x_dropoff =
                    (Int4) (extendParams->options->gap_x_dropoff_final * 
                    NCBIMATH_LN2 / 
                    searchParams->kbp_gap_orig->Lambda*localScalingFactor);
            } else {
                /* Lambda is already scaled */
                gapAlign->gap_x_dropoff =
                    (Int4) (extendParams->options->gap_x_dropoff_final * 
                    NCBIMATH_LN2 / kbp->Lambda);
            }
            BLAST_GappedAlignmentWithTraceback(program_number,
                                               query.data, subject.data,
                                               gapAlign, scoringParams,
                                               q_start, s_start,
                                               query.length, subject.length);

            newAlign = NewAlignmentFromGapAlign(gapAlign, window);
            withDistinctEnds(&newAlign, &alignments);
          } /* end if adjust search failed */
        } /* end if not isAlreadyContained */
      } /* for all HSPs in thisMatch */
      Kappa_SequenceDataRelease(&subject);
    } /* end else we are not performing a Smith-Waterman alignment */
    sfree(window_of_hsp);

    if( alignments != NULL) { /* alignments were found */
      BlastHSPList * hsp_list; /* a hitlist containing the newly-computed
                                * alignments */
      double  bestEvalue; /* best evalue among alignments in the hitlist */

      hsp_list = s_HSPListFromDistinctAlignments(&alignments,
                                                 matchingSeq.index);

      if(hsp_list->hspcnt > 1) { /* if there is more than one HSP, */
        /* then eliminate HSPs that are contained in a higher-scoring HSP. */
        if(!SmithWaterman || nWindows > 1) {
          /* For SmithWaterman alignments in a single window, the
           * forbidden ranges rule does not allow one alignment to be
           * contained in another, so the call to HitlistReapContained
           * is not needed. */
          qsort(hsp_list->hsp_array, hsp_list->hspcnt, sizeof(BlastHSP *),
                ScoreCompareHSPs);
          HitlistReapContained(hsp_list->hsp_array, &hsp_list->hspcnt);
        }
      }

      if(do_link_hsps) {
        BLAST_LinkHsps(program_number, hsp_list,
                       queryInfo, matchingSeq.length,
                       sbp, hitParams->link_hsp_params, TRUE);
      } else {
        Blast_HSPListGetEvalues(queryInfo, hsp_list, TRUE, sbp,
                                0.0, /* use a non-zero gap decay only when
                                       linking hsps */
                                1.0); /* Use scaling factor equal to 1, because 
                                         both scores and Lambda are scaled, so 
                                         they will cancel each other. */
      }
      bestEvalue = hsp_list->best_evalue;

      if(bestEvalue <= hitParams->options->expect_value &&
         SWheapWouldInsert(&significantMatches, bestEvalue)) {
        /* If the best alignment is significant, then save the current list */

        Blast_HSPListReapByEvalue(hsp_list, hitParams->options);

        s_HSPListRescaleScores(hsp_list, kbp->Lambda, kbp->logK,
                                 localScalingFactor);

        SWheapInsert(&significantMatches, hsp_list, bestEvalue,
                     thisMatch->oid);
      } else { /* the best alignment is not significant */
        Blast_HSPListFree(hsp_list);
      } /* end else the best alignment is not significant */
    } /* end if any alignments were found */

    Kappa_MatchingSequenceRelease(&matchingSeq);
    thisMatch = Blast_HSPListFree(thisMatch);
  }
  /* end for all matching sequences */
  SWheapToFlatList( &significantMatches, results,
                    hitParams->options->hitlist_size );
  /* Clean up */
  for( window_index = 0; window_index < nWindows; window_index++ ) {
    sfree(windows[window_index]);
  }
  sfree(windows);
  SWheapRelease(&significantMatches);
  if(SmithWaterman) Kappa_ForbiddenRangesRelease(&forbidden);
  gapAlign = BLAST_GapAlignStructFree(gapAlign);

  Kappa_RestoreSearch(searchParams, sbp, matrix, scoringParams, positionBased);
  Kappa_SearchParametersFree(&searchParams);

  return 0;
}
