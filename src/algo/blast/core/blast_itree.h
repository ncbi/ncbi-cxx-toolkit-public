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

/** @file blast_itree.h
 * Interface for an interval tree, used for fast HSP containment tests
 */

#ifndef ALGO_BLAST_CORE__BLAST_ITREE__H
#define ALGO_BLAST_CORE__BLAST_ITREE__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/core/blast_hits.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Structure describing a node of an interval tree. This is a 
    binary tree that organizes a collection of line segments 
    for fast overlap and containment tests. 
    
    Tree nodes contain a left subtree (containing HSPs whose query
    offsets all lie in the left half of the region described by an
    internal node), a right subtree, and a midpoint list. This last
    contains all HSPs whose query offsets would lie in both subtrees.
 
    For each non-leaf node, the midpoint list for query offsets is 
    itself organized as an interval tree (the "midpoint tree"), this 
    time indexing subject offsets. In this way the subject offsets act 
    as tiebreakers for the query offsets. HSPs in a given midpoint tree 
    that straddle both subtrees are collected into a linked list */
typedef struct SIntervalNode {
    Int4 leftend;   /**< The left endpoint of the region this node describes */
    Int4 rightend;  /**< The right endpoint of the region this node describes */
    Int4 leftptr;   /**< Offset to the subtree describing the left half
                         of the region, OR the query start offset (leaf 
                         nodes only) */
    Int4 midptr;    /**< Used for linked list of segments that cross the
                         center of the region */
    Int4 rightptr;  /**< Offset to the subtree describing the right half
                         of the region */
    BlastHSP *hsp;  /**< The HSP contained in this region (only non-NULL
                         for leaf nodes) */
} SIntervalNode;

/** Main structure describing an interval tree. */
typedef struct BlastIntervalTree {
    SIntervalNode *nodes;    /**< Pool of tree nodes to allocate from */
    Int4 num_alloc;          /**< Number of nodes allocated */
    Int4 num_used;           /**< Number of nodes actually in use */
    Int4 s_min;              /**< minimum subject offset possible */
    Int4 s_max;              /**< maximum subject offset possible */
} BlastIntervalTree;

/** How HSPs added to an interval tree are indexed */
typedef enum EITreeIndexMethod {
    eQueryOnly,              /**< Index by query offset only */
    eQueryAndSubject,        /**< Index by query and then by subject offset */
    eQueryOnlyStrandIndifferent /**< Index by query offset only.  Also do not
                                     distinguish between query strands for 
                                     region definition. -RMH- */
} EITreeIndexMethod;

/** Initialize an interval tree structure
 *  @param q_start Minimum query offset [in]
 *  @param q_end Maximum query offset; for multiple concatenated 
 *               queries, all sequences are combined [in]
 *  @param s_start Minimum subject offset [in]
 *  @param s_end Maximum subject offset [in]
 */
BlastIntervalTree* 
Blast_IntervalTreeInit(Int4 q_start, Int4 q_end,
                       Int4 s_start, Int4 s_end);

/** Deallocate an interval tree structure
 *  @param tree The tree to deallocate [in]
 *  @return Always NULL
 */
BlastIntervalTree* 
Blast_IntervalTreeFree(BlastIntervalTree *tree);

/** Empty an interval tree structure but do not free it.
 *  @param tree The tree to reset [in]
 *  @return Always NULL
 */
void
Blast_IntervalTreeReset(BlastIntervalTree *tree);

/** Add an HSP to an existing interval tree. 
 * @param hsp The HSP to add [in]
 * @param tree The tree to update [in][out]
 * @param query_info Structure with query offset information [in]
 * @param index_method How HSP will be indexed within the tree [in]
 * @return zero if succes, otherwise indicates an error
 */
Int2 
BlastIntervalTreeAddHSP(BlastHSP *hsp, 
                        BlastIntervalTree *tree,
                        const BlastQueryInfo *query_info,
                        EITreeIndexMethod index_method);

/** Determine whether an interval tree contains an HSP 
 *  that envelops an input HSP. An HSP is "contained" or 
 *  "enveloped" by another HSP if the query and subject offsets 
 *  of the first HSP lie completely within the query and subject 
 *  offsets of the second. If min_diag_separation is nonzero,
 *  containment is only signaled if an HSP in the tree lies
 *  within that many diagonals of an HSP in the tree.
 *  @param tree Interval tree to search [in]
 *  @param hsp The HSP used to query the tree [in]
 *  @param query_info Structure with query offset information [in]
 *  @param min_diag_separation Number of diagonals separating 
 *                             nonoverlapping hits (only nonzero 
 *                             for megablast) [in]
 *  @return TRUE if an HSP in the tree envelops the input, FALSE otherwise
 */
Boolean
BlastIntervalTreeContainsHSP(const BlastIntervalTree *tree,
                             const BlastHSP *hsp,
                             const BlastQueryInfo *query_info,
                             Int4 min_diag_separation);

/** Determine the number of HSPs within an interval tree whose
 *  query range envelops the input HSP. The tree is assumed to
 *  only index the query offsets of HSPs, and all HSPs within
 *  the tree are assumed to have score >= that of the input HSP.
 *  Finally, the interval tree is allowed to contain HSPs that
 *  describe hits to multiple different subject sequences.
 *  @param tree Interval tree to search [in]
 *  @param hsp The HSP used to query the tree [in]
 *  @param query_info Structure with query offset information [in]
 *  @return Number of HSPs in the tree whose query range envelops
 *          the query range of in_hsp
 */
Int4
BlastIntervalTreeNumRedundant(const BlastIntervalTree *tree, 
                              const BlastHSP *hsp,
                              const BlastQueryInfo *query_info);

// -RMH-
Int4
BlastIntervalTreeMasksHSP(const BlastIntervalTree *tree,
                              const BlastHSP *hsp,
                              const BlastQueryInfo *query_info,
                              Int4 subtree_index,
                              Int4 masklevel );


#ifdef __cplusplus
}
#endif
#endif /* !ALGO_BLAST_CORE__BLAST_ITREE__H */
