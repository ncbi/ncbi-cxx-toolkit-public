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
 *
 * Author: Jason Papadopoulos
 *
 */

/** @file blast_itree.c
 * Functions that implement an interval tree for fast HSP containment tests
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include "blast_itree.h"
#include "blast_gapalign_priv.h"
#include "blast_hits_priv.h"

/** When allocating a node for an interval tree, this
    specifies which half of the parent node will be described
    by the new node */
enum EIntervalDirection {
    eIntervalTreeLeft,    /**< Node will handle left half of parent node */
    eIntervalTreeRight,   /**< Node will handle right half of parent node */
    eIntervalTreeNeither  /**< No parent node is assumed */
};

/** Allocate a new node for an interval tree
 *  @param tree The tree to which the new node will eventually 
 *              be added [in][out]
 *  @param parent_index Offset of parent node to which this node 
                        will be attached  (may be 0) [in]
 *  @param dir Specifies which half of the parent node to describe [in]
 *  @return "Pointer" to the new node. Because allocations are made in
 *          batches, this is not a real pointer but the offset into the
 *          current pool of nodes
 */
static Int4
s_IntervalNodeInit(BlastIntervalTree *tree, 
                   Int4 parent_index,
                   enum EIntervalDirection dir,
                   Int2* ret_status)
{
    Int4 new_index;
    Int4 midpt;
    SIntervalNode *new_node;
    SIntervalNode *parent_node;

    *ret_status = 0;

    if (tree->num_used == tree->num_alloc) {
        tree->num_alloc = 2 * tree->num_alloc;
        tree->nodes = (SIntervalNode *)realloc(tree->nodes, tree->num_alloc *
                                                     sizeof(SIntervalNode));
    }

    if(tree->nodes == NULL)
    {
         *ret_status = BLASTERR_MEMORY;
         return 0;
    }

    new_index = tree->num_used++;
    if (dir == eIntervalTreeNeither)
        return new_index;

    /* fields in the node are only filled in if a parent
       node is specified */

    parent_node = tree->nodes + parent_index;
    new_node = tree->nodes + new_index;
    new_node->leftptr = 0;
    new_node->midptr = 0;
    new_node->rightptr = 0;
    new_node->hsp = NULL;
    midpt = (parent_node->leftend + parent_node->rightend) / 2;

    /* the endpoints of the new node depend on whether
       it is for the left or right subtree of the parent.
       The two subregions do not overlap, may be of length
       one, and must completely cover the parent region */

    if (dir == eIntervalTreeLeft) {
        new_node->leftend = parent_node->leftend;
        new_node->rightend = midpt;
    }
    else {
        new_node->leftend = midpt + 1;
        new_node->rightend = parent_node->rightend;
    }

    return new_index;
}

/** Allocate a new root node for an interval tree
 *  @param tree The tree to which the new node will eventually 
 *              be added [in][out]
 *  @param region_start The left endpoint of the root node
 *  @param region_end The right endpoint of the root node
 *  @return "Pointer" to the new node. Because allocations are made in
 *          batches, this is not a real pointer but the offset into the
 *          current pool of nodes
 */
static Int4
s_IntervalRootNodeInit(BlastIntervalTree *tree, 
                       Int4 region_start, Int4 region_end, Int2* retval)
{
    Int4 new_index;
    SIntervalNode *new_node;

    new_index = s_IntervalNodeInit(tree, 0, eIntervalTreeNeither, retval);
    if(*retval != 0)
       return 0;

    new_node = tree->nodes + new_index;
    new_node->leftptr = 0;
    new_node->midptr = 0;
    new_node->rightptr = 0;
    new_node->hsp = NULL;
    new_node->leftend = region_start;
    new_node->rightend = region_end;
    return new_index;
}

/* See blast_itree.h for description */
BlastIntervalTree* 
Blast_IntervalTreeInit(Int4 q_start, Int4 q_end,
                       Int4 s_start, Int4 s_end)
{
    Int4 size = 100;
    BlastIntervalTree *tree;
    Int2 retval=0;

    tree = (BlastIntervalTree *)malloc(sizeof(BlastIntervalTree));
    if (tree == NULL)
    {
          return NULL;
    }
    tree->nodes = (SIntervalNode *)malloc(size * sizeof(SIntervalNode));
    if (tree->nodes == NULL)
    {
          sfree(tree);
          return NULL;
    }
    tree->num_alloc = size;
    tree->num_used = 0;
    tree->s_min = s_start;
    tree->s_max = s_end;

    /* The first structure in tree->nodes is the root */
    s_IntervalRootNodeInit(tree, q_start, q_end, &retval);
    if(retval)
    {
       Blast_IntervalTreeFree(tree);
       return NULL;
    }
    return tree;
}

/* See blast_itree.h for description */
BlastIntervalTree* 
Blast_IntervalTreeFree(BlastIntervalTree *tree)
{
    if (tree == NULL)
        return NULL;

    sfree(tree->nodes);
    sfree(tree);
    return NULL;
}

/* See blast_itree.h for description */
void
Blast_IntervalTreeReset(BlastIntervalTree *tree)
{
    SIntervalNode *root = tree->nodes;

    tree->num_used = 1;
    root->leftptr = 0;
    root->midptr = 0;
    root->rightptr = 0;
    root->hsp = NULL;
}

/** Retrieves the start offset (within a set of concatentated query
 *  sequences) of the strand containing a given context
 *  @param query_info Information for all concatenated queries [in]
 *  @param context The context whose strand offset is required [in]
 *  @return Start offset of the strand of the query sequence
 *          containing 'context'
 */
static Int4 
s_GetQueryStrandOffset(const BlastQueryInfo *query_info,
                       Int4 context)
{
    Int4 c = context;

    while (c) {
        Int4 frame = query_info->contexts[c].frame;
        if (frame == 0 || SIGN(frame) != 
            SIGN(query_info->contexts[c-1].frame)) {
            break;
        }
        c--;
    }

    return query_info->contexts[c].query_offset;
}

/** Determine whether an input HSP shares a common start- or
 *  endpoint with an HSP from an interval tree.
 *  @param in_hsp The input HSP 
 *  @param in_q_start The start offset of the strand of the query
 *                    sequence containing in_hsp [in]
 *  @param tree_hsp An HSP from the interval tree [in]
 *  @param tree_q_start The start offset of the strand of the query
 *                    sequence containing tree_hsp [in]
 *  @param which_end Whether to match the left or right HSP endpoint [in]
 *  @return NULL if there is no common endpoint, otherwise a
 *          pointer to the HSP that should be kept
 */
static const BlastHSP*
s_HSPsHaveCommonEndpoint(const BlastHSP *in_hsp,
                         Int4 in_q_start,
                         const BlastHSP *tree_hsp,
                         Int4 tree_q_start,
                         enum EIntervalDirection which_end)
{
    Boolean match;

    /* check if alignments are from different query sequences 
       or query strands */

    if (in_q_start != tree_q_start)
        return NULL;
       
    /* check if alignments are from different subject strands */

    if (SIGN(in_hsp->subject.frame) != SIGN(tree_hsp->subject.frame))
        return NULL;
       
    if (which_end == eIntervalTreeLeft) {
        match = in_hsp->query.offset == tree_hsp->query.offset &&
                in_hsp->subject.offset == tree_hsp->subject.offset;
    }
    else {
        match = in_hsp->query.end == tree_hsp->query.end &&
                in_hsp->subject.end == tree_hsp->subject.end;
    }

    if (match) {
        Int4 in_q_length, tree_q_length, in_s_length, tree_s_length;

        /* keep the higher scoring HSP */

        if (in_hsp->score > tree_hsp->score)
            return in_hsp;
        if (in_hsp->score < tree_hsp->score)
            return tree_hsp;

        /* for equal scores, pick the shorter HSP */
        in_q_length = in_hsp->query.end - in_hsp->query.offset;
        tree_q_length = tree_hsp->query.end - tree_hsp->query.offset;
        if (in_q_length > tree_q_length)
            return tree_hsp;
        if (in_q_length < tree_q_length)
            return in_hsp;

        in_s_length = in_hsp->subject.end - in_hsp->subject.offset;
        tree_s_length = tree_hsp->subject.end - tree_hsp->subject.offset;
        if (in_s_length > tree_s_length)
            return tree_hsp;
        if (in_s_length < tree_s_length)
            return in_hsp;

        /* HSPs are identical; favor the one already in the tree */

        return tree_hsp;
    }

    return NULL;
}

/** Determine whether a subtree of an interval tree contains an HSP 
 *  that shares a common endpoint with the input HSP. The subtree
 *  indexes subject offsets, and represents the midpoint list of 
 *  a tree node that indexes query offsets
 *  @param tree Interval tree to search [in]
 *  @param root_index The offset into the list of tree nodes that
 *                    represents the root of the subtree [in]
 *  @param in_hsp The input HSP [in]
 *  @param in_q_start The start offset of the strand of the query
 *                    sequence containing in_hsp [in]
 *  @param which_end Whether to match the left or right HSP endpoint [in]
 *  @return TRUE if the HSP should not be added to the tree because
 *          it shares an existing endpoint with a 'better' HSP already
 *          there, FALSE if the HSP should still be added to the tree
 */
static Boolean
s_MidpointTreeHasHSPEndpoint(BlastIntervalTree *tree, 
                             Int4 root_index, 
                             const BlastHSP *in_hsp,
                             Int4 in_q_start,
                             enum EIntervalDirection which_end)
{
    SIntervalNode *root_node = tree->nodes + root_index;
    SIntervalNode *list_node, *next_node;
    Int4 tmp_index;
    Int4 target_offset;
    Int4 midpt;

    if (which_end == eIntervalTreeLeft)
        target_offset = in_hsp->subject.offset;
    else
        target_offset = in_hsp->subject.end;

    /* Descend the tree */

    while (1) {

        ASSERT(target_offset >= root_node->leftend);
        ASSERT(target_offset <= root_node->rightend);

        /* First perform matching endpoint tests on all of the HSPs
           in the midpoint list for the current node. If the input 
           shares an endpoint with an HSP already in the list, and the
           HSP in the list is 'better', signal that in_hsp should not
           be added to the tree later. Otherwise remove matching HSPs 
           from the list. */

        tmp_index = root_node->midptr;
        list_node = root_node;
        next_node = tree->nodes + tmp_index;
        while (tmp_index != 0) {
            const BlastHSP *best_hsp = s_HSPsHaveCommonEndpoint(in_hsp, 
                                                 in_q_start, next_node->hsp, 
                                                 next_node->leftptr, which_end);

            tmp_index = next_node->midptr;
            if (best_hsp == next_node->hsp)
                return TRUE;
            else if (best_hsp == in_hsp)
                list_node->midptr = tmp_index;

            list_node = next_node;
            next_node = tree->nodes + tmp_index;
        }

        /* Descend to the left or right subtree, whichever one
           contains the endpoint from in_hsp */

        tmp_index = 0;
        midpt = (root_node->leftend + root_node->rightend) / 2;
        if (target_offset < midpt)
            tmp_index = root_node->leftptr;
        else if (target_offset > midpt)
            tmp_index = root_node->rightptr;

        /* If there is no such subtree, then all of the HSPs that 
           could possibly have a common endpoint with it have already 
           been examined */

        if (tmp_index == 0)
            return FALSE;

        next_node = tree->nodes + tmp_index;
        if (next_node->hsp != NULL) {

            /* reached a leaf; compare in_hsp with the alignment
               in the leaf. Whether or not there's a match, traversal
               is finished */

            const BlastHSP *best_hsp = s_HSPsHaveCommonEndpoint(in_hsp, 
                                                 in_q_start, next_node->hsp, 
                                                 next_node->leftptr, which_end);
            if (best_hsp == next_node->hsp) {
                return TRUE;
            }
            else if (best_hsp == in_hsp) {
                /* leaf gets removed */
                if (target_offset < midpt)
                    root_node->leftptr = 0;
                else if (target_offset > midpt)
                    root_node->rightptr = 0;
                return FALSE;
            }
            break;
        }
        root_node = next_node;          /* descend to next node */
    }
    return FALSE;
}

/** Determine whether an interval tree contains one or more HSPs that 
 *  share a common endpoint with the input HSP. Remove from the tree
 *  all such HSPs that are "worse" than the input (do not delete the HSPs
 *  themselves)
 *  @param tree Interval tree to search [in]
 *  @param in_hsp The input HSP [in]
 *  @param in_q_start The start offset of the strand of the query
 *                    sequence containing in_hsp [in]
 *  @param which_end Whether to match the left or right HSP endpoint [in]
 *  @return TRUE if the HSP should not be added to the tree because
 *          it shares an existing endpoint with a 'better' HSP already
 *          there, FALSE if the HSP should still be added to the tree.
 *          Note it is possible for the input HSP to delete HSPs in the 
 *          tree but still have this routine return FALSE
 */
static Boolean
s_IntervalTreeHasHSPEndpoint(BlastIntervalTree *tree, 
                             const BlastHSP *in_hsp,
                             Int4 in_q_start,
                             enum EIntervalDirection which_end)
{
    SIntervalNode *root_node = tree->nodes;
    SIntervalNode *next_node;
    Int4 tmp_index;
    Int4 target_offset;
    Int4 midpt;

    if (which_end == eIntervalTreeLeft)
        target_offset = in_q_start + in_hsp->query.offset;
    else
        target_offset = in_q_start + in_hsp->query.end;

    /* Descend the tree */

    while (1) {

        ASSERT(target_offset >= root_node->leftend);
        ASSERT(target_offset <= root_node->rightend);

        /* First perform matching endpoint tests on all of the HSPs
           in the midpoint tree for the current node */

        tmp_index = root_node->midptr;
        if (tmp_index != 0) {
            if (s_MidpointTreeHasHSPEndpoint(tree, tmp_index, in_hsp, 
                                             in_q_start, which_end)) {
                return TRUE;
            }
        }

        /* Descend to the left or right subtree, whichever one
           contains the endpoint from in_hsp */

        tmp_index = 0;
        midpt = (root_node->leftend + root_node->rightend) / 2;
        if (target_offset < midpt)
            tmp_index = root_node->leftptr;
        else if (target_offset > midpt)
            tmp_index = root_node->rightptr;

        /* If there is no such subtree, or the HSP straddles the center
           of the current node, then all of the HSPs that could possibly
           have a common endpoint with it have already been examined */

        if (tmp_index == 0)
            return FALSE;

        next_node = tree->nodes + tmp_index;
        if (next_node->hsp != NULL) {

            /* reached a leaf; compare in_hsp with the alignment
               in the leaf. Whether or not there's a match, traversal
               is finished */

            const BlastHSP* best_hsp = s_HSPsHaveCommonEndpoint(in_hsp, 
                                              in_q_start, next_node->hsp, 
                                              next_node->leftptr, which_end);
            if (best_hsp == next_node->hsp) {
                return TRUE;
            }
            else if (best_hsp == in_hsp) {
                /* leaf gets removed */
                if (target_offset < midpt)
                    root_node->leftptr = 0;
                else if (target_offset > midpt)
                    root_node->rightptr = 0;
                return FALSE;
            }
            break;
        }
        root_node = next_node;          /* descend to next node */
    }
    return FALSE;
}


/* see blast_itree.h for description */
Int2 
BlastIntervalTreeAddHSP(BlastHSP *hsp, BlastIntervalTree *tree,
                        const BlastQueryInfo *query_info,
                        EITreeIndexMethod index_method)
{
    Int4 query_start;
    Int4 old_region_start;
    Int4 old_region_end;
    Int4 region_start;
    Int4 region_end;
    SIntervalNode *nodes;
    BlastHSP *old_hsp;
    Int4 root_index;
    Int4 new_index;
    Int4 mid_index;
    Int4 old_index;
    Int4 middle;
    enum EIntervalDirection which_half;
    Boolean index_subject_range = FALSE;
    Int2 retval = 0;

    /* Determine the query strand containing the input HSP.
       Only the strand matters for containment purposes,
       not the precise value of the query frame */

    query_start = s_GetQueryStrandOffset(query_info, hsp->context);

    if ( index_method == eQueryOnlyStrandIndifferent &&
         query_info->contexts[hsp->context].frame == -1 ) {
        /* translate the ranges to the + (frame=1) strand for storage
           and comparison. -RMH- */
        region_end = query_start - hsp->query.offset;
        region_start = query_start - hsp->query.end;
        query_start = query_start -
                      query_info->contexts[hsp->context].query_length - 1;
    }else {
        region_start = query_start + hsp->query.offset;
        region_end = query_start + hsp->query.end;
    }

    nodes = tree->nodes;
    ASSERT(region_start >= nodes->leftend);
    ASSERT(region_end <= nodes->rightend);
    ASSERT(hsp->query.offset <= hsp->query.end);
    ASSERT(hsp->subject.offset <= hsp->subject.end);

    if (index_method == eQueryAndSubject) {
            
        ASSERT(hsp->subject.offset >= tree->s_min);
        ASSERT(hsp->subject.end <= tree->s_max);
    
        /* Before adding the HSP, determine whether one or more
           HSPs already in the tree share a common endpoint with
           in_hsp. Remove from the tree any leaves containing 
           such an HSP whose score is lower than in_hsp.
    
           Note that in_hsp might share an endpoint with a
           higher-scoring HSP already in the tree, in which case
           in_hsp should not be added. There is thus a possibility
           that in_hsp will remove an alignment from the tree and
           then another alignment will remove in_hsp. This is arguably
           not the right behavior, but since the tree is only for
           containment tests the worst that can happen is that
           a rare extra gapped alignment will be computed */
    
        if (s_IntervalTreeHasHSPEndpoint(tree, hsp, query_start,
                                         eIntervalTreeLeft)) {
            return retval;
        }
        if (s_IntervalTreeHasHSPEndpoint(tree, hsp, query_start,
                                         eIntervalTreeRight)) {
            return retval;
        }
    }

    /* begin by indexing the HSP query offsets */

    index_subject_range = FALSE;

    /* encapsulate the input HSP in an SIntervalNode */
    root_index = 0;
    new_index = s_IntervalNodeInit(tree, 0, eIntervalTreeNeither, &retval);
    if (retval)
         return retval;
    nodes = tree->nodes;
    nodes[new_index].leftptr = query_start;
    nodes[new_index].midptr = 0;
    nodes[new_index].hsp = hsp;

    /* Descend the tree to reach the correct subtree for the new node */

    while (1) {

        ASSERT(region_start >= nodes[root_index].leftend);
        ASSERT(region_end <= nodes[root_index].rightend);

        middle = (nodes[root_index].leftend +
                  nodes[root_index].rightend) / 2;

        if (region_end < middle) {

            /* new interval belongs in left subtree. If there
               are no leaves in that subtree, finish up */

            if (nodes[root_index].leftptr == 0) {
                nodes[root_index].leftptr = new_index;
                return retval;
            }

            /* A node is already in this subtree. If it is not a 
               leaf node, descend to it and analyze in the next
               loop iteration. Otherwise, schedule the subtree to
               be split */

            old_index = nodes[root_index].leftptr;
            if (nodes[old_index].hsp == NULL) {
                root_index = old_index;
                continue;
            }
            else {
                which_half = eIntervalTreeLeft;
            }
        } 
        else if (region_start > middle) {

            /* new interval belongs in right subtree. If there
               are no leaves in that subtree, finish up */

            if (nodes[root_index].rightptr == 0) {
                nodes[root_index].rightptr = new_index;
                return retval;
            }

            /* A node is already in this subtree. If it is not a 
               leaf node, descend to it and analyze in the next
               loop iteration. Otherwise, schedule the subtree to
               be split */

            old_index = nodes[root_index].rightptr;
            if (nodes[old_index].hsp == NULL) {
                root_index = old_index;
                continue;
            }
            else {
                which_half = eIntervalTreeRight;
            }
        } 
        else {

            /* the new interval crosses the center of the node, and
               so has a "shadow" in both subtrees */

            // Added support for eQueryOnlyStrandIndifferent -RMH-
            if (index_subject_range || index_method == eQueryOnly ||
                index_method == eQueryOnlyStrandIndifferent ) {

                /* If indexing subject offsets already, prepend the 
                   new node to the list of "midpoint" nodes and return.
                   midptr is always a linked list if only the query
                   offsets are indexed */

                nodes[new_index].midptr = nodes[root_index].midptr;
                nodes[root_index].midptr = new_index;
                return retval;
            }
            else {

                /* Begin another tree at root_index, that indexes
                   the subject range of the input HSP */

                index_subject_range = TRUE;

                if (nodes[root_index].midptr == 0) {
                    mid_index = s_IntervalRootNodeInit(tree, tree->s_min,
                                                       tree->s_max, &retval);
                    if (retval)
                      return retval;   
                    nodes = tree->nodes;
                    nodes[root_index].midptr = mid_index;
                }
                root_index = nodes[root_index].midptr;

                /* switch from the query range of the input HSP 
                   to the subject range */

                region_start = hsp->subject.offset;
                region_end = hsp->subject.end;
                continue;
            }
        }

        /* There are two leaves in the same subtree. Add another
           internal node, reattach the old leaf, and loop 

           First allocate the new node. Update the pointer to 
           the pool of nodes, since it may change */

        mid_index = s_IntervalNodeInit(tree, root_index, which_half, &retval);
        if (retval)
          return retval;
        nodes = tree->nodes;
        old_hsp = nodes[old_index].hsp;

        /* attach the new internal node */

        if (which_half == eIntervalTreeLeft)
                nodes[root_index].leftptr = mid_index;
        else
                nodes[root_index].rightptr = mid_index;

        /* descend to the new internal node, and attach the old
           leaf to it. The next loop iteration will have to deal
           with attaching the *new* leaf */

        if (index_subject_range) {
            old_region_start = old_hsp->subject.offset;
            old_region_end = old_hsp->subject.end;
        }
        else {
            // -RMH-
            if ( index_method == eQueryOnlyStrandIndifferent &&
                 query_info->contexts[old_hsp->context].frame == -1 ) {
              /* Translate the old_hsp coordinates to the + strand 
                 (frame=1) for storage and comparison. -RMH- */
              Int4 q_start = s_GetQueryStrandOffset(query_info,
                                                    old_hsp->context);
              old_region_end = q_start - old_hsp->query.offset;
              old_region_start = q_start - old_hsp->query.end;
            }else {
              old_region_start = nodes[old_index].leftptr + old_hsp->query.offset;
              old_region_end = nodes[old_index].leftptr + old_hsp->query.end;
            }
        }

        root_index = mid_index;
        middle = (nodes[root_index].leftend +
                  nodes[root_index].rightend) / 2;
        if (old_region_end < middle) {

            /* old leaf belongs in left subtree of new node */
            nodes[mid_index].leftptr = old_index;
        }
        else if (old_region_start > middle) {

            /* old leaf belongs in right subtree of new node */
            nodes[mid_index].rightptr = old_index;
        }
        else {

            /* the old leaf straddles both subtrees. If indexing is
               by subject offset, attach the old leaf to the (empty)
               midpoint list of the new node. If still indexing query
               offsets, then a new tree that indexes subject offsets
               must be allocated from scratch, just to accomodate the
               old leaf */

            // Added support for eQueryOnlyStrandIndifferent -RMH-
            if (index_subject_range || index_method == eQueryOnly ||
                index_method == eQueryOnlyStrandIndifferent ) {
                nodes[mid_index].midptr = old_index;
            }
            else {
                Int4 mid_index2 = s_IntervalRootNodeInit(tree, tree->s_min,
                                                         tree->s_max, &retval);
                if (retval)
                      return retval;   
                old_region_start = old_hsp->subject.offset; 
                old_region_end =  old_hsp->subject.end;
                nodes = tree->nodes;
                nodes[mid_index].midptr = mid_index2;
                middle = (nodes[mid_index2].leftend +
                          nodes[mid_index2].rightend) / 2;
    
                if (old_region_end < middle)
                    nodes[mid_index2].leftptr = old_index;
                else if (old_region_start > middle)
                    nodes[mid_index2].rightptr = old_index;
                else
                    nodes[mid_index2].midptr = old_index;
            }
        }
    }
    return retval;
}

/** Determine whether an HSP is contained within another HSP.
 *  @param in_hsp The input HSP 
 *  @param in_q_start The start offset of the strand of the query
 *                    sequence containing in_hsp [in]
 *  @param tree_hsp An HSP from the interval tree [in]
 *  @param tree_q_start The start offset of the strand of the query
 *                      sequence containing tree_hsp [in]
 *  @param min_diag_separation Number of diagonals separating 
 *                             nonoverlapping hits (only nonzero 
 *                             for megablast) [in]
 *  @return TRUE if the second HSP envelops the first, FALSE otherwise
 */
static Boolean
s_HSPIsContained(const BlastHSP *in_hsp,
                 Int4 in_q_start,
                 const BlastHSP *tree_hsp,
                 Int4 tree_q_start,
                 Int4 min_diag_separation)
{
    /* check if alignments are from different query sequences 
       or query strands */

    if (in_q_start != tree_q_start)
        return FALSE;
       
    if (in_hsp->score <= tree_hsp->score &&
        SIGN(in_hsp->subject.frame) == SIGN(tree_hsp->subject.frame) &&
        CONTAINED_IN_HSP(tree_hsp->query.offset, tree_hsp->query.end, 
                              in_hsp->query.offset,
                              tree_hsp->subject.offset, tree_hsp->subject.end, 
                              in_hsp->subject.offset) &&
        CONTAINED_IN_HSP(tree_hsp->query.offset, tree_hsp->query.end, 
                             in_hsp->query.end,
                             tree_hsp->subject.offset, tree_hsp->subject.end, 
                             in_hsp->subject.end)) {

        if (min_diag_separation == 0)
            return TRUE;

        if (MB_HSP_CLOSE(tree_hsp->query.offset, tree_hsp->subject.offset,
                         in_hsp->query.offset, in_hsp->subject.offset,
                         min_diag_separation) ||
            MB_HSP_CLOSE(tree_hsp->query.end, tree_hsp->subject.end,
                         in_hsp->query.end, in_hsp->subject.end,
                         min_diag_separation)) {
            return TRUE;
        }
    }

    return FALSE;
}

/** Determine whether a subtree of an interval tree contains an HSP 
 *  that envelops the input HSP. The subtree indexes subject offsets,
 *  and represents the midpoint list of a tree node that indexes
 *  query offsets
 *  @param tree Interval tree to search [in]
 *  @param root_index The offset into the list of tree nodes that
 *                    represents the root of the subtree [in]
 *  @param in_hsp The input HSP [in]
 *  @param in_q_start The start offset of the strand of the query
 *                    sequence containing in_hsp [in]
 *  @param min_diag_separation Number of diagonals separating 
 *                             nonoverlapping hits (only nonzero 
 *                             for contiguous megablast) [in]
 *  @return TRUE if an HSP in the subtree envelops the input, FALSE otherwise
 */
static Boolean
s_MidpointTreeContainsHSP(const BlastIntervalTree *tree, 
                          Int4 root_index, 
                          const BlastHSP *in_hsp,
                          Int4 in_q_start,
                          Int4 min_diag_separation)
{
    SIntervalNode *node = tree->nodes + root_index;
    Int4 region_start = in_hsp->subject.offset;
    Int4 region_end = in_hsp->subject.end;
    Int4 middle;
    Int4 tmp_index = 0;

    /* Descend the tree */

    while (node->hsp == NULL) {

        ASSERT(region_start >= node->leftend);
        ASSERT(region_end <= node->rightend);

        /* First perform containment tests on all of the HSPs
           in the midpoint list for the current node. These
           HSPs are not indexed in a tree format, so all HSPs
           in the list must be examined */

        tmp_index = node->midptr;
        while (tmp_index != 0) {
            SIntervalNode *tmp_node = tree->nodes + tmp_index;

            if (s_HSPIsContained(in_hsp, in_q_start,
                                 tmp_node->hsp, tmp_node->leftptr,
                                 min_diag_separation)) {
                return TRUE;
            }
            tmp_index = tmp_node->midptr;
        }

        /* Descend to the left subtree if the input HSP lies completely
           to the left of this node's center, or to the right subtree if
           it lies completely to the right */

        tmp_index = 0;
        middle = (node->leftend + node->rightend) / 2;
        if (region_end < middle)
            tmp_index = node->leftptr;
        else if (region_start > middle)
            tmp_index = node->rightptr;

        /* If there is no such subtree, or the HSP straddles the center
           of the current node, then all of the HSPs that could possibly
           contain it have already been examined */

        if (tmp_index == 0)
            return FALSE;

        node = tree->nodes + tmp_index;
    }

    /* Reached a leaf of the tree */

    return s_HSPIsContained(in_hsp, in_q_start,
                            node->hsp, node->leftptr,
                            min_diag_separation);
}

/* see blast_itree.h for description */
Boolean
BlastIntervalTreeContainsHSP(const BlastIntervalTree *tree, 
                             const BlastHSP *hsp,
                             const BlastQueryInfo *query_info,
                             Int4 min_diag_separation)
{
    SIntervalNode *node = tree->nodes;
    Int4 query_start = s_GetQueryStrandOffset(query_info, hsp->context);
    Int4 region_start = query_start + hsp->query.offset;
    Int4 region_end = query_start + hsp->query.end;
    Int4 middle;
    Int4 tmp_index = 0;

    ASSERT(region_start >= node->leftend);
    ASSERT(region_end <= node->rightend);
    ASSERT(hsp->subject.offset >= tree->s_min);
    ASSERT(hsp->subject.end <= tree->s_max);
    ASSERT(hsp->query.offset <= hsp->query.end);
    ASSERT(hsp->subject.offset <= hsp->subject.end);

    /* Descend the tree */

    while (node->hsp == NULL) {

        ASSERT(region_start >= node->leftend);
        ASSERT(region_end <= node->rightend);

        /* First perform containment tests on all of the HSPs
           in the midpoint tree for the current node */

        tmp_index = node->midptr;
        if (tmp_index > 0) {
            if (s_MidpointTreeContainsHSP(tree, tmp_index, 
                                          hsp, query_start,
                                          min_diag_separation)) {
                return TRUE;
            }
        }

        /* Descend to the left subtree if the input HSP lies completely
           to the left of this node's center, or to the right subtree if
           it lies completely to the right */

        tmp_index = 0;
        middle = (node->leftend + node->rightend) / 2;
        if (region_end < middle)
            tmp_index = node->leftptr;
        else if (region_start > middle)
            tmp_index = node->rightptr;

        /* If there is no such subtree, or the HSP straddles the center
           of the current node, then all of the HSPs that could possibly
           contain it have already been examined */

        if (tmp_index == 0)
            return FALSE;

        node = tree->nodes + tmp_index;
    }

    /* Reached a leaf of the tree */

    return s_HSPIsContained(hsp, query_start, 
                            node->hsp, node->leftptr,
                            min_diag_separation);
}

/** Determine whether the query range of an HSP is contained 
 *  within the query range of another HSP.
 *  @param in_hsp The input HSP 
 *  @param in_q_start The start offset of the strand of the query
 *                    sequence containing in_hsp [in]
 *  @param tree_hsp An HSP from the interval tree, assumed to have
 *                  score equal to or exceeding that of in_hsp [in]
 *  @param tree_q_start The start offset of the strand of the query
 *                      sequence containing tree_hsp [in]
 *  @return 1 if query range of the second HSP envelops that of
 *           the first, 0 otherwise
 */
static Int4
s_HSPQueryRangeIsContained(const BlastHSP *in_hsp,
                           Int4 in_q_start,
                           const BlastHSP *tree_hsp,
                           Int4 tree_q_start)
{
    /* check if alignments are from different query sequences 
       or query strands. Also check if tree_hsp has score strictly
       higher than in_hsp */

    if (in_q_start != tree_q_start ||
        in_hsp->score >= tree_hsp->score)
        return 0;
       
    if (tree_hsp->query.offset <= in_hsp->query.offset &&
        tree_hsp->query.end >= in_hsp->query.end)
        return 1;

    return 0;
}

/** Determine whether the query range of an HSP overlaps 
 *  within the query range of another HSP by a percentage of
 *  their aligned lengths.
 *  
 *  This routine adds the cross_match -masklevel functionality
 *  to the Blast itree implementation
 *
 *  @param in_offset  The input HSP start position ( relative to in_q_start )
 *  @param in_end     The input HSP end position ( relative to in_q_start )
 *  @param in_score   The input HSP score
 *  @param in_q_start The start offset of the strand of the query
 *                    sequence containing in_hsp [in]
 *  @param tree_hsp An HSP from the interval tree, assumed to have
 *                  score equal to or exceeding that of in_hsp [in]
 *  @param tree_q_start The start offset of the strand of the query
 *                      sequence containing tree_hsp [in]
 *  @param masklevel    The percentage of either range which qualifies
 *                      the in_hsp as being part of the same query
 *                      domain group ( ie. "contained" in blast parlance ).
 *  @return 1 if query range of either HSP is masklevel% contained in
 *           that of the other -- higher score always wins.
 *  -RMH-
 */
static Int4
s_HSPQueryRangeIsMasklevelContained(Int4 in_offset,
                                    Int4 in_end,
                                    Int4 in_score,
                                    Int4 in_q_start,
                                    const BlastHSP *tree_hsp,
                                    Int4 tree_q_start,
                                    const BlastQueryInfo *query_info,
                                    Int4 masklevel )
{
    /* check if alignments are from different query sequences 
       or query strands. Also check if tree_hsp has score strictly
       higher than in_hsp */

    if (in_q_start != tree_q_start ||
        in_score > tree_hsp->score)
    {
        return 0;
    }

    Int4 tree_hsp_offset;
    Int4 tree_hsp_end;
    tree_q_start = s_GetQueryStrandOffset(query_info, tree_hsp->context);
    if ( query_info->contexts[tree_hsp->context].frame == -1 )
    {
      tree_hsp_end = tree_q_start - tree_hsp->query.offset;
      tree_hsp_offset = tree_q_start - tree_hsp->query.end;
    }else {
      tree_hsp_offset = tree_q_start + tree_hsp->query.offset;
      tree_hsp_end = tree_q_start + tree_hsp->query.end;
    }

    Int4 overlapStart = tree_hsp_offset;
    if ( overlapStart < in_offset )
      overlapStart = in_offset;
    Int4 overlapEnd = tree_hsp_end;
    if ( overlapEnd > in_end )
      overlapEnd = in_end;
    Int4 percOverlap = (Int4)( 100*((double)(overlapEnd - overlapStart) /
                               (in_end - in_offset)) );

    if ( percOverlap >=  masklevel )
      return 1;

    return 0;
}

// -RMH-
Int4
BlastIntervalTreeMasksHSP(const BlastIntervalTree *tree,
                              const BlastHSP *hsp,
                              const BlastQueryInfo *query_info,
                              Int4 subtree_index,
                              Int4 masklevel )
{
    Int4 region_start;
    Int4 region_end;
    SIntervalNode *node = tree->nodes + subtree_index;

    Int4 in_query_start = s_GetQueryStrandOffset(query_info, hsp->context);
    Int4 tmp_qstart = in_query_start;
    if ( query_info->contexts[hsp->context].frame == -1 )
    {
      region_end = in_query_start - hsp->query.offset;
      region_start = in_query_start - hsp->query.end;
      in_query_start = in_query_start - query_info->contexts[hsp->context].query_length - 1;
    }else {
      region_start = in_query_start + hsp->query.offset;
      region_end = in_query_start + hsp->query.end;
    }

    Int4 middle;
    Int4 tmp_index = 0;

    ASSERT(hsp->query.offset <= hsp->query.end);
    ASSERT(hsp->subject.offset <= hsp->subject.end);

    /* Descend the tree */

    while (node->hsp == NULL) {

        /* First perform containment tests on all of the HSPs
           in the midpoint list for the current node. All HSPs
           in the list must be examined */
        tmp_index = node->midptr;
        while (tmp_index != 0) {
            SIntervalNode *tmp_node = tree->nodes + tmp_index;
            if ( s_HSPQueryRangeIsMasklevelContained(region_start, region_end,
                                      hsp->score, in_query_start,
                                      tmp_node->hsp, tmp_node->leftptr,
                                      query_info,
                                      masklevel) )
              return 1;
            tmp_index = tmp_node->midptr;
        }

        /* Descend to the left subtree if the input HSP lies completely
           to the left of this node's center, or to the right subtree if
           it lies completely to the right */

        middle = (node->leftend + node->rightend) / 2;
        tmp_index = 0;
        if (region_end < middle)
            tmp_index = node->leftptr;
        else if (region_start > middle)
            tmp_index = node->rightptr;
        else
        {

         /* If the current region stradles the middle region it 
            still might be the case that there is an overlapping
            hit in either the right or left subtree.  Since we
            are not strictly looking for containment we must look
            for these higher scoring shorter segments either side. */
           if ( node->leftptr && BlastIntervalTreeMasksHSP( tree, hsp, query_info, node->leftptr, masklevel ) == 1 )
            return 1;
           if ( node->rightptr && BlastIntervalTreeMasksHSP( tree, hsp, query_info, node->rightptr, masklevel ) == 1 )
            return 1;
        }

        if (tmp_index == 0)
            return 0;

        node = tree->nodes + tmp_index;
    }

    /* Reached a leaf of the tree */
    return s_HSPQueryRangeIsMasklevelContained(region_start, region_end,
                                      hsp->score, in_query_start,
                                      node->hsp, node->leftptr,
                                      query_info,
                                      masklevel);

}

/* see blast_itree.h for description */
Int4
BlastIntervalTreeNumRedundant(const BlastIntervalTree *tree, 
                              const BlastHSP *hsp,
                              const BlastQueryInfo *query_info)
{
    SIntervalNode *node = tree->nodes;
    Int4 in_query_start = s_GetQueryStrandOffset(query_info, hsp->context);
    Int4 region_start = in_query_start + hsp->query.offset;
    Int4 region_end = in_query_start + hsp->query.end;
    Int4 middle;
    Int4 tmp_index = 0;
    Int4 num_redundant = 0;

    ASSERT(region_start >= node->leftend);
    ASSERT(region_end <= node->rightend);
    ASSERT(hsp->query.offset <= hsp->query.end);
    ASSERT(hsp->subject.offset <= hsp->subject.end);

    /* Descend the tree */

    while (node->hsp == NULL) {

        ASSERT(region_start >= node->leftend);
        ASSERT(region_end <= node->rightend);

        /* First perform containment tests on all of the HSPs
           in the midpoint list for the current node. All HSPs
           in the list must be examined */

        tmp_index = node->midptr;
        while (tmp_index != 0) {
            SIntervalNode *tmp_node = tree->nodes + tmp_index;

            num_redundant += s_HSPQueryRangeIsContained(hsp, in_query_start,
                                           tmp_node->hsp, tmp_node->leftptr);
            tmp_index = tmp_node->midptr;
        }

        /* Descend to the left subtree if the input HSP lies completely
           to the left of this node's center, or to the right subtree if
           it lies completely to the right */

        tmp_index = 0;
        middle = (node->leftend + node->rightend) / 2;
        if (region_end < middle)
            tmp_index = node->leftptr;
        else if (region_start > middle)
            tmp_index = node->rightptr;

        /* If there is no such subtree, or the HSP straddles the center
           of the current node, then all of the HSPs that could possibly
           contain it have already been examined */

        if (tmp_index == 0)
            return num_redundant;

        node = tree->nodes + tmp_index;
    }

    /* Reached a leaf of the tree */

    return num_redundant + 
           s_HSPQueryRangeIsContained(hsp, in_query_start,
                                      node->hsp, node->leftptr);
}
