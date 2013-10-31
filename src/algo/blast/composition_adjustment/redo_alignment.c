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

/** @file redo_alignment.c
 * Routines for redoing a set of alignments, using either
 * composition matrix adjustment or the Smith-Waterman algorithm (or
 * both.)
 *
 * @author Alejandro Schaffer, E. Michael Gertz
 */
#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/composition_adjustment/redo_alignment.h>
#include <algo/blast/composition_adjustment/nlm_linear_algebra.h>
#include <algo/blast/composition_adjustment/composition_adjustment.h>
#include <algo/blast/composition_adjustment/composition_constants.h>
#include <algo/blast/composition_adjustment/smith_waterman.h>
#include <algo/blast/composition_adjustment/compo_heap.h>

/** The natural log of 2, defined in newer systems as M_LN2 in math.h, but
    missing in older systems. */
#define LOCAL_LN2 0.69314718055994530941723212145818

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

/** by what factor might initially reported E-value exceed true E-value */
#define EVALUE_STRETCH 5

/** -1/0/1 if a is less than/greater than/equal to b */
#ifndef CMP
#define CMP(a,b) ((a)>(b) ? 1 : ((a)<(b) ? -1 : 0))
#endif

/** Test of whether one set of HSP bounds is contained in another */
#define KAPPA_CONTAINED_IN_HSP(a,b,c,d,e,f) \
((a <= c && b >= c) && (d <= f && e >= f))
/** A macro that defines the mathematical "sign" function */
#define KAPPA_SIGN(a) (((a) > 0) ? 1 : (((a) < 0) ? -1 : 0))

/** For translated subject sequences, the number of amino acids to
  include before and after the existing aligned segment when
  generating a composition-based scoring system. */
static const int kWindowBorder = 200;

/** pseudocounts for relative-entropy-based score matrix adjustment */
static const int kReMatrixAdjustmentPseudocounts = 20;

/**
 * s_WindowInfo - a struct whose instances represent a range
 * of data in a sequence. */
typedef struct s_WindowInfo
{
    BlastCompo_SequenceRange query_range;   /**< range of the query
                                            included in this window */
    BlastCompo_SequenceRange subject_range; /**< range of the subject
                                            included in this window */
    BlastCompo_Alignment * align;   /**< list of existing alignments
                                            contained in this window */
    int hspcnt;                        /**< number of alignment in
                                            this window */
} s_WindowInfo;


/* Documented in redo_alignment.h. */
BlastCompo_Alignment *
BlastCompo_AlignmentNew(int score,
                        EMatrixAdjustRule matrix_adjust_rule,
                        int queryStart, int queryEnd, int queryIndex,
                        int matchStart, int matchEnd, int frame,
                        void * context)
{
    BlastCompo_Alignment * align = malloc(sizeof(BlastCompo_Alignment));
    if (align != NULL) {
        align->score = score;
        align->matrix_adjust_rule = matrix_adjust_rule;
        align->queryIndex = queryIndex;
        align->queryStart = queryStart;
        align->queryEnd = queryEnd;
        align->matchStart = matchStart;
        align->matchEnd = matchEnd;
        align->frame = frame;
        align->context = context;
        align->next = NULL;
    }
    return align;
}


/* Documented in redo_alignment.h. */
void
BlastCompo_AlignmentsFree(BlastCompo_Alignment ** palign,
                             void (*free_context)(void*))
{
    BlastCompo_Alignment * align;      /* represents the current
                                             alignment in loops */
    align = *palign;  *palign = NULL;
    while (align != NULL) {
        /* Save the value of align->next, because align is to be deleted. */
        BlastCompo_Alignment * align_next = align->next;

        align_next = align->next;
        if (free_context != NULL && align->context != NULL) {
            free_context(align->context);
        }
        free(align);
        align = align_next;
    }
}


/**
 * Reverse a list of BlastCompo_Alignments. */
static void
s_AlignmentsRev(BlastCompo_Alignment ** plist)
{
    BlastCompo_Alignment *list;       /* the forward list */
    BlastCompo_Alignment *new_list;   /* the reversed list */
    list = *plist; new_list = NULL;
    while (list != NULL) {
        BlastCompo_Alignment * list_next = list->next;
        list->next = new_list;
        new_list = list;
        list = list_next;
    }
    *plist = new_list;
}


/**
 * Compare two BlastCompo_Alignments. */
static int
s_AlignmentCmp(const BlastCompo_Alignment * a,
                           const BlastCompo_Alignment * b)
{
    int result;
    if (0 == (result = CMP(b->score, a->score)) &&
        0 == (result = CMP(a->matchStart, b->matchStart)) &&
        0 == (result = CMP(b->matchEnd, a->matchEnd)) &&
        0 == (result = CMP(a->queryStart, b->queryStart))) {
        /* if all other tests cannot distinguish the alignments, then
         * the final test is the result */
        result = CMP(b->queryEnd, a->queryEnd);
    }
    return result;
}

/** Temporary function to determine whether alignments are sorted */
static int
s_AlignmentsAreSorted(BlastCompo_Alignment * alignments)
{
    BlastCompo_Alignment * align;
    for (align = alignments;  align != NULL;  align = align->next) {
        if (align->next && align->next->score > align->score) {
            return 0;
        }
    }
    return 1;
}


/** Calculate the length of a list of BlastCompo_Alignment objects.
 *  This is an O(n) operation */
static int
s_DistinctAlignmentsLength(BlastCompo_Alignment * list) 
{
    int length = 0;
    for ( ;  list != NULL;  list = list->next) {
        length++;
    }
    return length;
}


/**
 * Sort a list of Blast_Compo_Alignment objects, using s_AlignmentCmp 
 * comparison function.  The mergesort sorting algorithm is used.
 *
 * @param *plist        the list to be sorted
 * @param hspcnt        the length of the list
 */
static void
s_DistinctAlignmentsSort(BlastCompo_Alignment ** plist, int hspcnt)
{
    if (COMPO_INTENSE_DEBUG) {
        assert(s_DistinctAlignmentsLength(*plist) == hspcnt);
    }
    if(hspcnt > 1) {
        BlastCompo_Alignment * list = *plist;
        BlastCompo_Alignment *leftlist, *rightlist, **tail;
        int i, leftcnt, rightcnt;
        
        /* Split the list in half */
        leftcnt = hspcnt/2;
        rightcnt = hspcnt - leftcnt;

        leftlist = list;
        /* Find the point to split the list; this loop splits lists
           correctly only when list != NULL and leftcnt > 0, which is
           necessarily the case because hspcnt > 1 */
        assert(list != NULL && leftcnt > 0);
        for (i = 0;  i < leftcnt - 1 && list->next != NULL;  i++) {
          list = list->next;
        }
        rightlist = list->next;
        list->next = NULL;

        if (COMPO_INTENSE_DEBUG) {
            assert(s_DistinctAlignmentsLength(rightlist) == rightcnt);
            assert(s_DistinctAlignmentsLength(leftlist) == leftcnt);
        }
        /* Sort the two lists */
        if (leftcnt > 1) 
            s_DistinctAlignmentsSort(&leftlist, leftcnt);
        if (rightcnt > 1)
            s_DistinctAlignmentsSort(&rightlist, rightcnt);

        /* And then merge them */
        list = NULL;
        tail = &list;
        while (leftlist != NULL || rightlist != NULL) {
            if (leftlist == NULL) {
                *tail = rightlist;
                rightlist = NULL;
            } else if (rightlist == NULL) {
                *tail = leftlist;
                leftlist = NULL;
            } else {
                BlastCompo_Alignment * elt;
                if (s_AlignmentCmp(leftlist, rightlist) < 0) {
                    elt = leftlist;
                    leftlist = leftlist->next;
                } else {
                    elt = rightlist;
                    rightlist = rightlist->next;
                }
                *tail = elt;
                tail = &elt->next;
            }
        }
        *plist = list;
        if (COMPO_INTENSE_DEBUG) {
            assert(s_DistinctAlignmentsLength(list) == hspcnt);
            assert(s_AlignmentsAreSorted(list));
        }
    }
}


/**
 * Copy a BlastCompo_Alignment, setting the next field to NULL
 */
static BlastCompo_Alignment *
s_AlignmentCopy(const BlastCompo_Alignment * align)
{
    return BlastCompo_AlignmentNew(align->score,
                                   align->matrix_adjust_rule,
                                   align->queryStart,
                                   align->queryEnd,
                                   align->queryIndex,
                                   align->matchStart,
                                   align->matchEnd, align->frame,
                                   align->context);
    
}

/**
 * Do given alignments have the same left or right end point
 */
static Boolean
s_IsSameEndPoint(const BlastCompo_Alignment* newAlign,
                 const BlastCompo_Alignment* align)
{
    ASSERT(newAlign->frame == align->frame);
    return ((align->queryStart == newAlign->queryStart &&
             align->matchStart == newAlign->matchStart)
            || (align->queryEnd == newAlign->queryEnd &&
                align->matchEnd == newAlign->matchEnd));
}


/**
 * Does at least one end point of newAlign is contined within align and lies
 * on the same diagnol as the same end point of align
 */
static Boolean
s_IsSimilarEndPoint(const BlastCompo_Alignment* newAlign,
                    const BlastCompo_Alignment* align)
{
    /* ASSERT(newAlign->frame == align->frame); */
    /* is start of newAlign contained within align */
    Boolean start_contained =
        KAPPA_CONTAINED_IN_HSP(align->queryStart, align->queryEnd,
                               newAlign->queryStart,
                               align->matchStart, align->matchEnd,
                               newAlign->matchStart);
    /* is end of newAlign contained within align */
    Boolean end_contained = 
        KAPPA_CONTAINED_IN_HSP(align->queryStart, align->queryEnd,
                               newAlign->queryEnd,
                               align->matchStart, align->matchEnd,
                               newAlign->matchEnd);

    /* does either end point of newAlign lie on the same diagonal as the same
       end point of align */
    Boolean result =  (start_contained && 
                       (newAlign->queryStart - newAlign->matchStart ==
                        align->queryStart - align->matchStart)) ||
                      (end_contained &&
                       (newAlign->queryEnd - newAlign->matchEnd ==
                        align->queryEnd - align->matchEnd));
       
    return result;
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
 * through a call to s_WithDistinctEnds. All alignments in the resulting
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
 * @param free_align_context    a routine to be used to free the context 
 *                              field of an alignment, if any alignment is
 *                              freed; may be NULL
 * @param is_same_adjustment    were all compared HSPs computed with the same
 *                              composition adjustment; Nearly identical
 *                              alignments are computed with exact subjects,
 *                              and others with segged subjects, hence nearly
 *                              identical alignments may extend farther; this
 *                              makes comparing end points more difficult
 */
static void
s_WithDistinctEnds(BlastCompo_Alignment **p_newAlign,
                   BlastCompo_Alignment **p_oldAlignments,
                   void free_align_context(void *),
                   Boolean is_same_adjustment)
{
    /* Deference the input parameters. */
    BlastCompo_Alignment * newAlign      = *p_newAlign;
    BlastCompo_Alignment * oldAlignments = *p_oldAlignments;
    BlastCompo_Alignment * align;      /* represents the current
                                             alignment in loops */
    int include_new_align;                /* true if the new alignment
                                             may be added to the list */
    *p_newAlign        = NULL;
    include_new_align  = 1;

    for (align = oldAlignments;  align != NULL;  align = align->next) {

        /* If all HSPs were computed with the same adjustment (and subject
           segging), then simply compare end points. Otherwise, check if the
           end point of the new HSPs is contained within an old one and
           whether it lies on the same diagonal as the end point of the old
           HSP. */

        if (align->frame == newAlign->frame &&
            ((is_same_adjustment && s_IsSameEndPoint(newAlign, align)) ||
             (!is_same_adjustment && s_IsSimilarEndPoint(newAlign, align)))) {
            /* At least one of the endpoints of newAlign matches an endpoint
               of align. */
            if (newAlign->score <= align->score) {
                /* newAlign cannot be added to the list. */
                include_new_align = 0;
                break;
            }
        }
    }
    if (include_new_align) {
        /* tail of the list being created */
        BlastCompo_Alignment **tail;

        tail  = &newAlign->next;
        align = oldAlignments;
        while (align != NULL) {
            /* Save align->next because align may be deleted. */
            BlastCompo_Alignment * align_next = align->next;
            align->next = NULL;
            if (align->frame == newAlign->frame &&
                ((align->queryStart == newAlign->queryStart &&
                  align->matchStart == newAlign->matchStart)
                 || (align->queryEnd == newAlign->queryEnd &&
                     align->matchEnd == newAlign->matchEnd))) {
                /* The alignment shares an end with newAlign; */
                /* delete it. */
                BlastCompo_AlignmentsFree(&align, free_align_context);
            } else { /* The alignment does not share an end with newAlign; */
                /* add it to the output list. */
                *tail =  align;
                tail  = &align->next;
            }
            align = align_next;
        } /* end while align != NULL */
        *p_oldAlignments = newAlign;
    } else { /* do not include_new_align */
        BlastCompo_AlignmentsFree(&newAlign, free_align_context);
    } /* end else do not include newAlign */
}
 

/** Release the data associated with this object. */
static void s_SequenceDataRelease(BlastCompo_SequenceData * self)
{
    if (self->buffer) free(self->buffer);
    self->data = NULL;  self->buffer = NULL;
}



/**
 * Create and initialize a new s_WindowInfo.
 *
 * Parameters to this function correspond directly to fields of
 * s_WindowInfo.
 */
static s_WindowInfo *
s_WindowInfoNew(int begin, int end, int context,
                    int queryOrigin, int queryLength, int query_index,
                    BlastCompo_Alignment * align)
{
    s_WindowInfo * window;    /* new window to be returned */

    window  = malloc(sizeof(s_WindowInfo));
    if (window != NULL) {
        window->subject_range.begin   = begin;
        window->subject_range.end     = end;
        window->subject_range.context = context;
        window->query_range.begin     = queryOrigin;
        window->query_range.end       = queryOrigin + queryLength;
        window->query_range.context   = query_index;
        window->align       = align;
        window->hspcnt      = 0;
        for ( ;  align != NULL;  align = align->next) {
            window->hspcnt++;
        }
    }
    return window;
}


/**
 * Swap the query and subject range
 *
 */
static void
s_WindowSwapRange(s_WindowInfo * self)
{
    BlastCompo_SequenceRange range;
    range.begin   = self->subject_range.begin;
    range.end     = self->subject_range.end;
    range.context = self->subject_range.context;
    self->subject_range.begin   = self->query_range.begin;
    self->subject_range.end     = self->query_range.end;
    self->subject_range.context = self->query_range.context;
    self->query_range.begin   = range.begin;
    self->query_range.end     = range.end;
    self->query_range.context = range.context;
    return;
}


/**
 * Free an instance of s_WindowInfo.
 *
 * @param *window   on entry the window to be freed; on exit NULL
 */
static void
s_WindowInfoFree(s_WindowInfo ** window)
{
    if (*window != NULL) {
        BlastCompo_AlignmentsFree(&(*window)->align, NULL);
        free(*window);
    }
    *window = NULL;
}


/**
 * Join two instance of s_WindowInfo into a single window
 *
 * @param win1      on entry, one of the two windows to be joined; on exit
 *                  the combined window
 * @param *pwin2    on entry, the other window to be joined, on exit NULL
 */
static void
s_WindowInfoJoin(s_WindowInfo * win1, s_WindowInfo ** pwin2)
{
    /* the second window, which will be deleted when this routine exits */
    s_WindowInfo * win2 = *pwin2;   
    BlastCompo_Alignment *align, **tail;
    /* subject ranges for the two windows */
    BlastCompo_SequenceRange * sbjct_range1 = &win1->subject_range;
    BlastCompo_SequenceRange * sbjct_range2 = &win2->subject_range;

    assert(sbjct_range1->context == sbjct_range2->context);
    assert(win1->query_range.context == win2->query_range.context);

    sbjct_range1->begin = MIN(sbjct_range1->begin, sbjct_range2->begin);
    sbjct_range1->end   = MAX(sbjct_range1->end, sbjct_range2->end);
    win1->hspcnt += win2->hspcnt;

    tail = &win1->align;
    for (align = win1->align;  align != NULL;  align = align->next) {
        tail = &align->next;
    }
    *tail = win2->align;
    win2->align = NULL;

    s_WindowInfoFree(pwin2);
}


/**
 * A comparison routine used to sort a list of windows, first by frame
 * and then by location.
 */
static int
s_LocationCompareWindows(const void * vp1, const void *vp2)
{
    /* w1 and w2 are the windows being compared */
    s_WindowInfo * w1 = *(s_WindowInfo **) vp1;
    s_WindowInfo * w2 = *(s_WindowInfo **) vp2;
    /* the subject ranges of the two windows */
    BlastCompo_SequenceRange * sr1 = &w1->subject_range;
    BlastCompo_SequenceRange * sr2 = &w2->subject_range;
    /* the query indices of the two windows */
    /* the query ranges of the two windows */
    BlastCompo_SequenceRange * qr1 = &w1->query_range;
    BlastCompo_SequenceRange * qr2 = &w2->query_range;

    int result;                   /* result of the comparison */
    if (0 == (result = CMP(qr1->context, qr2->context)) &&
        0 == (result = CMP(sr1->context, sr2->context)) && 
        0 == (result = CMP(sr1->begin, sr2->begin)) &&
        0 == (result = CMP(sr1->end, sr2->end)) &&
        0 == (result = CMP(qr1->begin, qr2->begin))) {
        result = CMP(qr1->end, qr2->end);
    }
    return result;
}


/**
 * A comparison routine used to sort a list of windows by position in
 * the subject, ignoring strand and frame. Ties are broken
 * deterministically. 
 */
static int
s_SubjectCompareWindows(const void * vp1, const void *vp2)
{
    /* w1 and w2 are the windows being compared */
    s_WindowInfo * w1 = *(s_WindowInfo **) vp1;
    s_WindowInfo * w2 = *(s_WindowInfo **) vp2;
    /* the subject ranges of the two windows */
    BlastCompo_SequenceRange * sr1 = &w1->subject_range;
    BlastCompo_SequenceRange * sr2 = &w2->subject_range;
    /* the query ranges of the two windows */
    BlastCompo_SequenceRange * qr1 = &w1->query_range;
    BlastCompo_SequenceRange * qr2 = &w2->query_range;

    int result;                   /* result of the comparison */
    if (0 == (result = CMP(sr1->begin, sr2->begin)) &&
        0 == (result = CMP(sr1->end, sr2->end)) &&
        0 == (result = CMP(sr1->context, sr2->context)) &&
        0 == (result = CMP(qr1->begin, qr2->begin)) &&
        0 == (result = CMP(qr1->end, qr2->end))) {
        result = CMP(qr1->context, qr2->context);
    }
    return result;
}

/**
 * Read a list of alignments from a translated search and create a
 * new array of pointers to s_WindowInfo so that each alignment is
 * contained in exactly one window. See s_WindowsFromAligns for the
 * meaning of the parameters. (@sa s_WindowsFromAligns).
 *
 * @return 0 on success, -1 on out-of-memory
 */
static int
s_WindowsFromTranslatedAligns(BlastCompo_Alignment * alignments,
                              BlastCompo_QueryInfo * query_info,
                              int hspcnt, int border, int sequence_length,
                              s_WindowInfo ***pwindows, int * nWindows,
                              int subject_is_translated)
{
    int k;                            /* iteration index */
    s_WindowInfo ** windows;      /* the output list of windows */
    int length_joined;                /* the current length of the
                                         list of joined windows */
    BlastCompo_Alignment * align;  /* represents the current
                                         alignment in the main loop */
    *nWindows = 0;
    windows = *pwindows = calloc(hspcnt, sizeof(s_WindowInfo*));
    *nWindows = hspcnt;
    if (windows == NULL)
        goto error_return;

    for (align = alignments, k = 0;
         align != NULL;
         align = align->next, k++) {
        int frame;             /* translation frame */
        int query_index;       /* index of the query contained in the
                                  current HSP */
        int query_length;      /* length of the current query */
        int translated_length; /* length of the translation of the entire
                                  nucleotide sequence in this frame */
        int begin, end;        /* interval in amino acid coordinates of
                                  the translated window */
        /* copy of the current alignment to add to the window */
        BlastCompo_Alignment * align_copy;
        frame = align->frame;
        query_index = align->queryIndex;
        query_length = query_info[query_index].seq.length;
        translated_length = (sequence_length - ABS(frame) + 1)/3;

        align_copy = s_AlignmentCopy(align);
        if (align_copy == NULL)
            goto error_return;

        if (subject_is_translated) {
            begin = MAX(0, align->matchStart - border);
            end   = MIN(translated_length, align->matchEnd + border);
            windows[k] = s_WindowInfoNew(begin, end, frame, 0,
                                query_length, query_index, align_copy);
        } else {
            begin = MAX(0, align->queryStart - border);
            end   = MIN(query_length, align->queryEnd + border);
            /* for blastx, temporarily swap subject and query ranges*/
            windows[k] = s_WindowInfoNew(begin, end, query_index, 0,
                                sequence_length, 0, align_copy);
        }
        if (windows[k] == NULL)
            goto error_return;
    }
    qsort(windows, hspcnt, sizeof(BlastCompo_SequenceRange*),
        s_LocationCompareWindows);

    /* Join windows that overlap or are too close together.  */
    length_joined = 0;
    for (k = 0;  k < hspcnt;  k++) {       /* for all windows in the
                                              original list */
        s_WindowInfo * window;          /* window at this value of k */
        s_WindowInfo * nextWindow;      /* window at the next
                                               value of k, or NULL if
                                               no such window
                                               exists */
        window     = windows[k];
        nextWindow = ( k + 1 < hspcnt ) ? windows[k+1] : NULL;

        if(nextWindow != NULL &&
           window->subject_range.context ==
           nextWindow->subject_range.context &&
           window->query_range.context == nextWindow->query_range.context &&
           window->subject_range.end >= nextWindow->subject_range.begin) {
            /* Join the current window with the next window.  Do not add the
               current window to the output list. */
            s_WindowInfoJoin(nextWindow, &windows[k]);
        } else {
            /* Don't join the current window with the next window.  Add the
               current window to the output list instead */
            windows[length_joined] = window;
            length_joined++;
        } /* end else don't join the current window with the next window */
    } /* end for all windows in the original list */
    *nWindows = length_joined;

    for (k = length_joined;  k < hspcnt;  k++) {
        windows[k] = NULL;
    }
 
    /* for blastx, swap query and subject range */
    if (!subject_is_translated) {   
        for (k=0; k<length_joined; k++) {
            s_WindowSwapRange(windows[k]);
        }
    }
            
    for (k = 0;  k < length_joined;  k++) {
        s_DistinctAlignmentsSort(&windows[k]->align, windows[k]->hspcnt);
    }
    qsort(windows, *nWindows, sizeof(BlastCompo_SequenceRange*),
          s_SubjectCompareWindows);
    return 0; /* normal return */

error_return:
    for (k = 0; k < *nWindows; k++) {
        if (windows[k] != NULL) 
            s_WindowInfoFree(&windows[k]);
    }
    free(windows);
    *pwindows = NULL;
    return -1;
}


/**
 * Read a list of alignments from a protein search and create a
 * new array of pointers to s_WindowInfo so that each alignment is
 * contained in exactly one window.  See s_WindowsFromAligns for the
 * meaning of the parameters. (@sa s_WindowsFromAligns).
 *
 * @return 0 on success, -1 on out-of-memory
 */
static int
s_WindowsFromProteinAligns(BlastCompo_Alignment * alignments,
                           BlastCompo_QueryInfo * query_info,
                           int numQueries,
                           int sequence_length,
                           s_WindowInfo ***pwindows,
                           int * nWindows)
{
    BlastCompo_Alignment * align;
    int query_index;   /* index of the query */
    int query_length;  /* length of an individual query */
    int window_index;  /* index of a window in the window list */

    /* new list of windows */
    s_WindowInfo ** windows =
        calloc(numQueries, sizeof(s_WindowInfo*));
    *nWindows = 0;
    if (windows == NULL)
        goto error_return;
    *nWindows = numQueries;
    for (align = alignments;  align != NULL;  align = align->next) {
        BlastCompo_Alignment * copiedAlign;

        query_index = align->queryIndex;
        query_length = query_info[query_index].seq.length;

        if (windows[query_index] == NULL) {
            windows[query_index] =
                s_WindowInfoNew(0, sequence_length, 0, 
                                0, query_length, query_index, NULL);
            if (windows[query_index] == NULL) 
                goto error_return;
        }
        copiedAlign = s_AlignmentCopy(align);
        if (copiedAlign == NULL) 
            goto error_return;
        copiedAlign->next = windows[query_index]->align;
        windows[query_index]->align = copiedAlign;
        windows[query_index]->hspcnt++;
    }
    window_index = 0;
    for (query_index = 0;  query_index < numQueries;  query_index++) {
        if (windows[query_index] != NULL) {
            windows[window_index] = windows[query_index];
            s_AlignmentsRev(&windows[window_index]->align);
            window_index++;
        }
    }
    /* shrink to fit */
    {
        s_WindowInfo ** new_windows =
            realloc(windows, window_index * sizeof(BlastCompo_SequenceRange*));
        if (new_windows == NULL) {
            goto error_return;
        } else {
            windows = new_windows;
            *nWindows = window_index;
        }
    }
    qsort(windows, *nWindows, sizeof(BlastCompo_SequenceRange*),
          s_SubjectCompareWindows);
    *pwindows = windows;
    /* Normal return */
    return 0;

error_return:
    for (window_index = 0;  window_index < *nWindows;  window_index++) {
        s_WindowInfoFree(&windows[window_index]);
    }
    free(windows);
    return -1;
}


/**
 * Read a list of alignments from a search (protein or translated) and
 * create a new array of pointers to s_WindowInfo so that each
 * alignment is contained in exactly one window.
 *
 * @param alignments        a list of alignments from a translated
 *                          search
 * @param query_info        information about the query/queries used
 *                          in the search
 * @param hspcnt            number of alignments
 * @param numQueries        number of queries
 * @param border            border around windows; windows with
 *                          overlapping borders will be joined.
 * @param sequence_length   length of the subject sequence, in
 *                          nucleotides for translated searches or
 *                          in amino acids for protein searches
 * @param *pwindows         the new array of windows
 * @param nWindows          the length of *pwindows
 * @param subject_is_translated    is the subject sequence translated?
 *
 * @return 0 on success, -1 on out-of-memory
 */
static int
s_WindowsFromAligns(BlastCompo_Alignment * alignments,
                BlastCompo_QueryInfo * query_info, int hspcnt,
                int numQueries, int border, int sequence_length,
                s_WindowInfo ***pwindows, int * nWindows,
                int query_is_translated, int subject_is_translated) 
{
    if (subject_is_translated || query_is_translated) {
        return s_WindowsFromTranslatedAligns(alignments, query_info,
                                             hspcnt, border,
                                             sequence_length,
                                             pwindows, nWindows,
                                             subject_is_translated);
    } else {
        return s_WindowsFromProteinAligns(alignments, query_info,
                                          numQueries, sequence_length,
                                          pwindows, nWindows);
    }
}


/**
 * Compute the amino acid composition of the sequence.
 *
 * @param composition          the computed composition.
 * @param alphsize             the size of the alphabet
 * @param seq                  subject/query sequence data
 * @param range                the range of the given sequence data in
 *                             the complete sequence
 * @param align                an alignment of the query to the
 *                             subject range
 * @param for_subject          is this done for subject or for query?
 */
static void
s_GetComposition(Blast_AminoAcidComposition * composition,
                 int alphsize,
                 BlastCompo_SequenceData * seq,
                 BlastCompo_SequenceRange * range,
                 BlastCompo_Alignment * align,
                 Boolean query_is_translated,
                 Boolean subject_is_translated)
{
    Uint1 * data;     /* sequence data for the subject */
    int length;       /* length of the subject portion of the alignment */
    /* [left, right) is the interval of the subject to use when
     * computing composition. The endpoints are offsets into the
     * subject_range. */
    int left, right;

    data = seq->data;
    length = range->end - range->begin;
   
    if (query_is_translated || subject_is_translated) {
        int start;  
        int end; 
        start = ((query_is_translated) ?  
                 align->queryStart : align->matchStart) - range->begin;
        end   = ((query_is_translated) ?  
                 align->queryEnd   : align->matchEnd  ) - range->begin;
        Blast_GetCompositionRange(&left, &right, data, length, start, end);
    } else {
        /* Use the whole subject to compute the composition */
        left = 0;
        right = length;
    }
    Blast_ReadAaComposition(composition, alphsize, &data[left], right-left);
}


/**
 * Compute an e-value from a score and a set of statistical parameters
 */
static double
s_EvalueFromScore(int score, double Lambda, double logK, double searchsp)
{
    return searchsp * exp(-(Lambda * score) + logK);
}


/**
 * The number of bits by which the score of a previously computed
 * alignment must exceed the score of the HSP under consideration for
 * a containment relationship to be reported by the isContained
 * routine. */
#define KAPPA_BIT_TOL 2.0

/**
 * Return true if an alignment is contained in a previously-computed
 * alignment of sufficiently high score.
 *
 * @param in_align            the alignment to be tested
 * @param alignments          list of alignments
 * @param lambda              Karlin-Altschul statistical parameter
 */
static Boolean
s_IsContained(BlastCompo_Alignment * in_align,
              BlastCompo_Alignment * alignments,
              double lambda)
{
    BlastCompo_Alignment * align;     /* represents the current alignment
                                            in the main loop */
    /* Endpoints of the alignment */
    int query_offset    = in_align->queryStart;
    int query_end       = in_align->queryEnd;
    int subject_offset  = in_align->matchStart;
    int subject_end     = in_align->matchEnd;
    double score        = in_align->score;
    double scoreThresh = score + KAPPA_BIT_TOL * LOCAL_LN2/lambda;

    for (align = alignments;  align != NULL;  align = align->next ) {
        /* for all elements of alignments */
        if (KAPPA_SIGN(in_align->frame) == KAPPA_SIGN(align->frame)) {
            /* hsp1 and hsp2 are in the same query/subject frame */
            if (KAPPA_CONTAINED_IN_HSP
                (align->queryStart, align->queryEnd, query_offset,
                 align->matchStart, align->matchEnd, subject_offset) &&
                KAPPA_CONTAINED_IN_HSP
                (align->queryStart, align->queryEnd, query_end,
                 align->matchStart, align->matchEnd, subject_end) &&
                scoreThresh <= align->score) {
                return 1;
            }
        }
    }
    return 0;
}


/* Documented in redo_alignment.h. */
void
Blast_RedoAlignParamsFree(Blast_RedoAlignParams ** pparams)
{
    if (*pparams != NULL) {
        Blast_MatrixInfoFree(&(*pparams)->matrix_info);
        free((*pparams)->gapping_params);
        free(*pparams);
        *pparams = NULL;
    }
}


/* Documented in redo_alignment.h. */
Blast_RedoAlignParams *
Blast_RedoAlignParamsNew(Blast_MatrixInfo ** pmatrix_info,
                         BlastCompo_GappingParams ** pgapping_params,
                         ECompoAdjustModes compo_adjust_mode,
                         int positionBased,
                         int query_is_translated,
                         int subject_is_translated,
                         int ccat_query_length, int cutoff_s,
                         double cutoff_e, int do_link_hsps,
                         const Blast_RedoAlignCallbacks * callbacks,
                         double near_identical_cutoff)
{
    Blast_RedoAlignParams * params = malloc(sizeof(Blast_RedoAlignParams));
    if (params) {
        params->matrix_info = *pmatrix_info;
        *pmatrix_info = NULL;
        params->gapping_params = *pgapping_params;
        *pgapping_params = NULL;

        params->compo_adjust_mode = compo_adjust_mode;
        params->positionBased = positionBased;
        params->RE_pseudocounts = kReMatrixAdjustmentPseudocounts;
        params->query_is_translated = query_is_translated;
        params->subject_is_translated = subject_is_translated;
        params->ccat_query_length = ccat_query_length;
        params->cutoff_s = cutoff_s;
        params->cutoff_e = cutoff_e;
        params->do_link_hsps = do_link_hsps;
        params->callbacks = callbacks;
        params->near_identical_cutoff = near_identical_cutoff;
    } else {
        free(*pmatrix_info); *pmatrix_info = NULL;
        free(*pgapping_params); *pgapping_params = NULL;
    }
    return params;
}

#define MINIMUM_LENGTH_NEAR_IDENTICAL 50


static Boolean s_preliminaryTestNearIdentical(BlastCompo_QueryInfo query_info[], 
                                              BlastCompo_Alignment* align,
                                              double cutoff)
{
  int queryIndex, queryLength, align_len;

  queryIndex = align->queryIndex;
  queryLength = query_info[queryIndex].seq.length;
  if ((align->matchEnd - align->matchStart +1) <
      (MIN(queryLength,  MINIMUM_LENGTH_NEAR_IDENTICAL)))
    return(FALSE);

  align_len = MIN(align->queryEnd - align->queryStart,
                  align->matchEnd - align->matchStart);

  if ((double)align->score / (double)align_len < cutoff) {
      return (FALSE);
  }

  return(TRUE);
}

/* Documented in redo_alignment.h. */
int
Blast_RedoOneMatch(BlastCompo_Alignment ** alignments,
                   Blast_RedoAlignParams * params,
                   BlastCompo_Alignment * incoming_aligns, int hspcnt,
                   double Lambda,
                   BlastCompo_MatchingSequence * matchingSeq,
                   int ccat_query_length, BlastCompo_QueryInfo query_info[],
                   int numQueries, int ** matrix, int alphsize,
                   Blast_CompositionWorkspace * NRrecord,
                   double *pvalueForThisPair,
                   int compositionTestIndex,
                   double *LambdaRatio)
{
    int status = 0;                  /* return status */
    s_WindowInfo **windows;      /* array of windows */
    int nWindows;                    /* length of windows */
    int window_index;                /* loop index */
    int query_index;                 /* index of the current query */
    /* which mode of composition adjustment is actually used? */
    EMatrixAdjustRule matrix_adjust_rule = eDontAdjustMatrix;

    /* fields of params, as local variables */
    Blast_MatrixInfo * scaledMatrixInfo = params->matrix_info;
    ECompoAdjustModes compo_adjust_mode = params->compo_adjust_mode;
    int RE_pseudocounts = params->RE_pseudocounts;
    int query_is_translated = params->query_is_translated;
    int subject_is_translated = params->subject_is_translated;
    BlastCompo_GappingParams * gapping_params = params->gapping_params;
    const Blast_RedoAlignCallbacks * callbacks = params->callbacks;

    assert((int) compo_adjust_mode < 2 || !params->positionBased);
    for (query_index = 0;  query_index < numQueries;  query_index++) {
        alignments[query_index] = NULL;
    }
    status =
        s_WindowsFromAligns(incoming_aligns, query_info, hspcnt, numQueries,
                            kWindowBorder, matchingSeq->length, &windows,
                            &nWindows, query_is_translated, subject_is_translated);
    if (status != 0) {
        goto function_level_cleanup;
    }
    /* for all windows */
    for (window_index = 0;  window_index < nWindows;  window_index++) {
        s_WindowInfo * window;   /* the current window */
        BlastCompo_Alignment * in_align;  /* the current alignment */
        int hsp_index;               /* index of the current alignment */
        /* data for the current window */
        BlastCompo_SequenceData subject = {0,};
        BlastCompo_SequenceData query = {0,}; 
        /* the composition of this query */
        Blast_AminoAcidComposition * query_composition;  
        Boolean nearIdenticalStatus; /* are query and subject nearly
                                        identical in the aligned part?*/
        Boolean oldNearIdenticalStatus; /* were query and subject nearly
                                           identical for previous alignmnet
                                           between the same query and subject*/

        Boolean subject_maybe_biased = TRUE; /* False means that the subject
                                                sequence does not have biased
                                                segments. True means we do
                                                not know */
        int num_adjustments = 0; /* number of matrix adjustments done in this
                                    window; nearly identical alignments may be
                                    computed with different composition
                                    adjustment than other alignments due to using
                                    full subjects instead of segged ones */

        window = windows[window_index];
        query_index = window->align->queryIndex;
        query_composition = &query_info[query_index].composition;

        /* for all alignments in this window */
        for (in_align = window->align, hsp_index = 0;
             in_align != NULL;
             in_align = in_align->next, hsp_index++) {


            /* if the fist HSP for the pair or subject may have biased
               segments; if the subject is not biased, then segging is not
               necessary so we do not have to check for near identical status */
            if (hsp_index == 0 || subject_maybe_biased) {

                nearIdenticalStatus = s_preliminaryTestNearIdentical(query_info,  
                                             in_align,
                                             params->near_identical_cutoff);
            }

            /* if the first HSP for the pair or subject may have biased
               segments and near identical status is different than for the
               previous HSP */
            if (hsp_index == 0 ||
                (subject_maybe_biased &&
                 (nearIdenticalStatus != oldNearIdenticalStatus))) {
 
                /* release previously allocated sequence data */
                s_SequenceDataRelease(&subject);
                s_SequenceDataRelease(&query);
                status =
                    callbacks->get_range(matchingSeq, &window->subject_range,
                                 &subject, 
                                 &query_info[query_index].seq,
                                 &window->query_range,
                                 &query,
                                 query_info[query_index].words,
                                 window->align, nearIdenticalStatus,
                                 compo_adjust_mode, FALSE,
                                 &subject_maybe_biased);

                if (status != 0) {
                    goto window_index_loop_cleanup;
                }
            }

            /* do frequency count for partial translated query */
            if (query_is_translated) {
                s_GetComposition(query_composition,
                                        alphsize, &query,
                                        &window->query_range,
                                        in_align, TRUE, FALSE);
            }
            /* if in_align is not contained in a higher-scoring
             * alignment */
            if ( !s_IsContained(in_align, alignments[query_index], Lambda) ) {
                BlastCompo_Alignment * newAlign;   /* the new alignment */
                /* adjust_search_failed is true only if Blast_AdjustScores
                 * is called and returns a nonzero value */
                int adjust_search_failed = 0;
                if (compo_adjust_mode != eNoCompositionBasedStats &&
                    (subject_is_translated || hsp_index == 0
                     || (nearIdenticalStatus != oldNearIdenticalStatus))) {
                    Blast_AminoAcidComposition subject_composition;
                    s_GetComposition(&subject_composition,
                                            alphsize, &subject,
                                            &window->subject_range,
                                            in_align, FALSE, subject_is_translated);
                    adjust_search_failed =
                        Blast_AdjustScores(matrix, query_composition,
                                           query.length,
                                           &subject_composition,
                                           subject.length,
                                           scaledMatrixInfo, compo_adjust_mode,
                                           RE_pseudocounts, NRrecord,
                                           &matrix_adjust_rule,
                                           callbacks->calc_lambda,
                                           pvalueForThisPair,
                                           compositionTestIndex,
                                           LambdaRatio);
                    if (adjust_search_failed < 0) { /* fatal error */
                        status = adjust_search_failed;
                        goto window_index_loop_cleanup;
                    }
                    num_adjustments++;
                }

                if ( !adjust_search_failed ) {
                    newAlign =
                        callbacks->
                        redo_one_alignment(in_align, matrix_adjust_rule,
                                           &query, &window->query_range,
                                           ccat_query_length,
                                           &subject, &window->subject_range,
                                           matchingSeq->length,
                                           gapping_params);
                    if (newAlign && newAlign->score >= params->cutoff_s) {
                        s_WithDistinctEnds(&newAlign, &alignments[query_index],
                                           callbacks->free_align_traceback,
                                           num_adjustments == 1);
                    } else {
                        BlastCompo_AlignmentsFree(&newAlign,
                                                  callbacks->
                                                  free_align_traceback);
                    }
                }
            } /* end if in_align is not contained...*/
            oldNearIdenticalStatus = nearIdenticalStatus;
        } /* end for all alignments in this window */
window_index_loop_cleanup:
        if (subject.data != NULL)
            s_SequenceDataRelease(&subject);
        if (query.data != NULL)
            s_SequenceDataRelease(&query);
        if (status != 0) 
            goto function_level_cleanup;
    } /* end for all windows */
function_level_cleanup:
    if (status != 0) {
        for (query_index = 0;  query_index < numQueries;  query_index++) {
            BlastCompo_AlignmentsFree(&alignments[query_index], 
                                         callbacks->free_align_traceback);
        }
    }
    for (window_index = 0;  window_index < nWindows;  window_index++) {
        s_WindowInfoFree(&windows[window_index]);
    }
    free(windows);
    
    return status;
}


/* Documented in redo_alignment.h. */
int
Blast_RedoOneMatchSmithWaterman(BlastCompo_Alignment ** alignments,
                                Blast_RedoAlignParams * params,
                                BlastCompo_Alignment * incoming_aligns,
                                int hspcnt,
                                double Lambda, double logK,
                                BlastCompo_MatchingSequence * matchingSeq,
                                BlastCompo_QueryInfo query_info[],
                                int numQueries,
                                int ** matrix, int alphsize,
                                Blast_CompositionWorkspace * NRrecord,
                                Blast_ForbiddenRanges * forbidden,
                                BlastCompo_Heap * significantMatches,
                                double *pvalueForThisPair,
                                int compositionTestIndex,
                                double *LambdaRatio)
{
    int status = 0;                     /* status return value */
    s_WindowInfo **windows = NULL;  /* array of windows */
    int nWindows;                       /* length of windows */
    int window_index;                   /* loop index */
    int query_index;                    /* index of the current query */
    /* which mode of composition adjustment is actually used? */
    EMatrixAdjustRule matrix_adjust_rule = eDontAdjustMatrix;

    /* fields of params, as local variables */
    Blast_MatrixInfo * scaledMatrixInfo = params->matrix_info;
    ECompoAdjustModes compo_adjust_mode = params->compo_adjust_mode;
    int positionBased = params->positionBased;
    int RE_pseudocounts = params->RE_pseudocounts;
    int query_is_translated = params->query_is_translated;
    int subject_is_translated = params->subject_is_translated;
    int do_link_hsps = params->do_link_hsps;
    int ccat_query_length = params->ccat_query_length;
    BlastCompo_GappingParams * gapping_params = params->gapping_params;
    const Blast_RedoAlignCallbacks * callbacks = params->callbacks;

    int gap_open = gapping_params->gap_open;
    int gap_extend = gapping_params->gap_extend;

    assert((int) compo_adjust_mode < 2 || !positionBased);
    for (query_index = 0;  query_index < numQueries;  query_index++) {
        alignments[query_index] = NULL;
    }
    /* Find the multiple translation windows used by tblastn queries. */
    status =
        s_WindowsFromAligns(incoming_aligns, query_info, hspcnt, numQueries,
                            kWindowBorder, matchingSeq->length, &windows,
                            &nWindows, query_is_translated, subject_is_translated);
    if (status != 0) 
        goto function_level_cleanup;
    /* We are performing a Smith-Waterman alignment */
    for (window_index = 0;  window_index < nWindows;  window_index++) {
        /* for all window */
        s_WindowInfo * window = NULL; /* the current window */
        BlastCompo_SequenceData subject = {0,}; 
        /* subject data for this window */
        BlastCompo_SequenceData query = {0,};       /* query data for this window */
        Blast_AminoAcidComposition * query_composition;  
        double searchsp;                  /* effective search space */
	Boolean nearIdenticalStatus; /*are query and subject nearly
				       identical in the aligned part?*/
        /* adjust_search_failed is true only if Blast_AdjustScores
         * is called and returns a nonzero value */
        int adjust_search_failed = FALSE;

        
        window = windows[window_index];
        query_index = window->query_range.context;
        query_composition = &query_info[query_index].composition;

        nearIdenticalStatus = s_preliminaryTestNearIdentical(query_info,  
                                             window->align,
                                             params->near_identical_cutoff);

        status =
            callbacks->get_range(matchingSeq, &window->subject_range,
                                 &subject, 
                                 &query_info[query_index].seq,
                                 &window->query_range,
                                 &query,
                                 query_info[query_index].words,
                                 window->align, nearIdenticalStatus,
                                 compo_adjust_mode, TRUE,
                                 NULL);

        if (status != 0) 
            goto window_index_loop_cleanup;
            
        /* do frequency count for partial translated query */
        if (query_is_translated) {
            s_GetComposition(query_composition,
                                    alphsize, &query,
                                    &window->query_range,
                                    window->align, TRUE, FALSE);
        }
        searchsp = query_info[query_index].eff_search_space;

        /* For Smith-Waterman alignments, adjust the search using the
         * composition of the highest scoring alignment in window */
        if (compo_adjust_mode != eNoCompositionBasedStats) {
            Blast_AminoAcidComposition subject_composition;
            s_GetComposition(&subject_composition, alphsize,
                                    &subject, &window->subject_range,
                                    window->align, FALSE, subject_is_translated);
            adjust_search_failed =
                Blast_AdjustScores(matrix,
                                   query_composition, query.length,
                                   &subject_composition, subject.length,
                                   scaledMatrixInfo, compo_adjust_mode,
                                   RE_pseudocounts, NRrecord,
                                   &matrix_adjust_rule, callbacks->calc_lambda,
                                   pvalueForThisPair,
                                   compositionTestIndex,
                                   LambdaRatio);
            if (adjust_search_failed < 0) { /* fatal error */
                status = adjust_search_failed;
                goto window_index_loop_cleanup;
            }
        }
        if ( !adjust_search_failed ) {
            /* BlastCompo_AdjustSearch ran without error; compute the new
               alignments. */
            int aSwScore;             /* score computed by the
                                       * Smith-Waterman algorithm. */
            int alignment_is_significant; /* True if the score/evalue of
                                           * the Smith-Waterman alignment
                                           * is significant. */
            Blast_ForbiddenRangesClear(forbidden);
            do {
                int matchEnd, queryEnd;    /* end points of the alignments
                                            * computed by the Smith-Waterman
                                            * algorithm. */
                status =
                    Blast_SmithWatermanScoreOnly(&aSwScore, &matchEnd,
                                                 &queryEnd,
                                                 subject.data,
                                                 subject.length,
                                                 query.data,
                                                 query.length, matrix,
                                                 gap_open, gap_extend,
                                                 positionBased,
                                                 forbidden);
                if (status != 0)
                    goto window_index_loop_cleanup;

                if (do_link_hsps) {
                    alignment_is_significant = aSwScore >= params->cutoff_s;
                } else {
                    double newSwEvalue;     /* evalue as computed by the
                                             * Smith-Waterman algorithm */
                    newSwEvalue =
                        s_EvalueFromScore(aSwScore, Lambda, logK, searchsp);

                    alignment_is_significant = newSwEvalue < params->cutoff_e;
                    if (alignments[query_index] == NULL) {
                        /* this is the most significant alignment; if
                         * it will not be accepted, no alignments from
                         * this match will */
                        alignment_is_significant =
                            alignment_is_significant &&
                            BlastCompo_HeapWouldInsert(
                                &significantMatches[query_index],
                                newSwEvalue, aSwScore, matchingSeq->index);
                    }
                }
                if (alignment_is_significant) {
                    /* the redone alignment */
                    BlastCompo_Alignment * newAlign;
                    int matchStart, queryStart;  /* the start of the
                                                  * alignment in the
                                                  * match/query sequence */
                    int updatedScore;            /* score found by the SW
                                                    algorithm run in reverse */
                    status =
                        Blast_SmithWatermanFindStart(&updatedScore,
                                                     &matchStart,
                                                     &queryStart,
                                                     subject.data,
                                                     subject.length,
                                                     query.data,
                                                     matrix, gap_open,
                                                     gap_extend,
                                                     matchEnd,
                                                     queryEnd,
                                                     aSwScore,
                                                     positionBased,
                                                     forbidden);
                    if (status != 0) {
                        goto window_index_loop_cleanup;
                    }
                    status =
                        callbacks->
                        new_xdrop_align(&newAlign, &queryEnd, &matchEnd,
                                        queryStart, matchStart, aSwScore,
                                        &query, &window->query_range,
                                        ccat_query_length,
                                        &subject, &window->subject_range,
                                        matchingSeq->length,
                                        gapping_params, matrix_adjust_rule);
                    if (status != 0) {
                        goto window_index_loop_cleanup;
                    }
                    newAlign->next = alignments[query_index];
                    alignments[query_index] = newAlign;

                    if (window->hspcnt > 1) {
                        /* We may compute more alignments; make the range
                           of the current alignment forbidden */
                        status =
                            Blast_ForbiddenRangesPush(forbidden,
                                                      queryStart, queryEnd,
                                                      matchStart, matchEnd);
                    }
                    if (status != 0) {
                        goto window_index_loop_cleanup;
                    }
                }
                /* end if the next local alignment is significant */
            } while (alignment_is_significant && window->hspcnt > 1);
            /* end do..while the next local alignment is significant, and
             * the original blast search found more than one alignment. */
        } /* end if BlastCompo_AdjustSearch ran without error.  */
window_index_loop_cleanup:
        if (subject.data != NULL)
            s_SequenceDataRelease(&subject);
        if (query.data != NULL)
            s_SequenceDataRelease(&query);
        if (status != 0) 
            goto function_level_cleanup;
    } /* end for all windows */
    
function_level_cleanup:
    if (status != 0) {
        for (query_index = 0;  query_index < numQueries;  query_index++) {
            BlastCompo_AlignmentsFree(&alignments[query_index],
                                      callbacks->free_align_traceback);
        }
    }
    for (window_index = 0;  window_index < nWindows;  window_index++) {
        s_WindowInfoFree(&windows[window_index]);
    }
    free(windows);
    
    return status;
}


/* Documented in redo_alignment.h. */
int
BlastCompo_EarlyTermination(double evalue,
                            BlastCompo_Heap significantMatches[],
                            int numQueries)
{
    int i;
    for (i = 0;  i < numQueries;  i++) {
        if (BlastCompo_HeapFilledToCutoff(&significantMatches[i])) {
            double ecutoff = significantMatches[i].ecutoff;
            /* Only matches with evalue <= ethresh will be saved. */
            if (evalue <= EVALUE_STRETCH * ecutoff) {
                /* The evalue if this match is sufficiently small
                 * that we want to redo it to try to obtain an
                 * alignment with evalue smaller than ecutoff. */
                return FALSE;
            }
        } else {
            return FALSE;
        }
    }
    return TRUE;
}
