static char const rcsid[] = "$Id$";

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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

/*****************************************************************************

File name: seg.cpp

Author: Jason Papadopoulos

Contents: Code that deals with scoring and manipulating
         alignments ("segments") from BLAST runs

******************************************************************************/

#include <ncbi_pch.hpp>
#include <algo/cobalt/cobalt.hpp>

/// @file seg.cpp
/// Code that deals with scoring and manipulating
/// alignments ("segments") from BLAST runs

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

typedef struct SAlignHalf {
    int hit_num;
    int seq;
    int other_seq;
    TRange range;
} SAlignHalf;

/// callback structure used to sort lists of segments;
/// sort by query index, using start offset and then end
/// offset as a tiebreaker. For identical segments, use
/// the other half of the alignment as a tiebreaker
///
struct compare_align_start {
    bool operator()(const SAlignHalf& a, const SAlignHalf& b) const {
        if (a.seq < b.seq)
            return true;
        if (a.seq > b.seq)
            return false;

        return (a.range.GetFrom() < b.range.GetFrom());
    }
};

/// Compute the hit rate for each member of a collection of segments
/// @param num_queries The number of input sequences [in]
/// @param align_list The collection of segments [in/modified]
///
void 
CMultiAligner::x_AssignHitRate()
{
    // The 'hit rate' for a segment is a scaling factor for
    // the score of that segment, defined as the sum over
    // all sequences of maxrate(i), i.e. the maximum percentage 
    // of that segment that appears in any alignment occurring in
    // the i_th sequence.
    //
    // This is an attempt to reward segments that are repeated
    // in multiple query sequences (or, more accurately,
    // to penalize segments that do not do so).

    int num_queries = m_QueryData.size();
    int num_hits = m_CombinedHits.Size();
    int num_segs = 2 * num_hits;
    CNcbiMatrix<int> best_overlap(num_segs, num_queries, 0);
    vector<SAlignHalf> seg_list(num_segs);

    for (int i = 0; i < num_hits; i++) {
        CHit *hit = m_CombinedHits.GetHit(i);
        seg_list[2*i].hit_num = 2 * i;
        seg_list[2*i].seq = hit->m_SeqIndex1;
        seg_list[2*i].other_seq = hit->m_SeqIndex2;
        seg_list[2*i].range = hit->m_SeqRange1;

        seg_list[2*i+1].hit_num = 2 * i + 1;
        seg_list[2*i+1].seq = hit->m_SeqIndex2;
        seg_list[2*i+1].other_seq = hit->m_SeqIndex1;
        seg_list[2*i+1].range = hit->m_SeqRange2;
    }

    sort(seg_list.begin(), seg_list.end(), compare_align_start());

    // For each segment in the list

    for (int i = 0; i < num_segs; i++) {

        // Walk forward from the segment in question until either
        // the list runs out, we use up all the segments on one 
        // query sequence, or segments in the list stop overlapping
        // with the target segment. For each segment that qualifies, 
        // compute the amount of overlap with the target segment and 
        // update the current best hit rate for the query sequence
        // to which the segment belongs. Note that a segment will
        // always contribute to its own hitrate

        for (int j = i; j < num_segs &&
                     seg_list[j].seq == seg_list[i].seq &&
                     seg_list[j].range.GetFrom() <= 
                             seg_list[i].range.GetTo(); j++) {
            SAlignHalf& align(seg_list[j]);
            int overlap = min(seg_list[i].range.GetTo(), 
                              align.range.GetTo()) - 
                          align.range.GetFrom() + 1;
            best_overlap(align.hit_num, seg_list[i].other_seq) = 
                   max(best_overlap(align.hit_num, seg_list[i].other_seq), overlap);
        }
    }

    // sum up all the best per-sequence rates, scale and set the hit rate.

    for (int i = 0; i < num_hits; i++) {
        double sum1 = 0.0, sum2 = 0.0;
        CHit *hit = m_CombinedHits.GetHit(i);
        for (int j = 0; j < num_queries; j++) {
            sum1 += (double)best_overlap(2*i, j);
            sum2 += (double)best_overlap(2*i+1, j);
        }
        sum1 = (sum1 / hit->m_SeqRange1.GetLength() + 1) / num_queries;
        sum2 = (sum2 / hit->m_SeqRange2.GetLength() + 1) / num_queries;
        hit->m_HitRate = 0.5 * (sum1 + sum2);

        ASSERT(hit->m_HitRate >= 1.0 / num_queries && hit->m_HitRate <= 1.0);
    }
}

/// Find a maximum weight path in a directed acyclic graph
/// @param nodes The graph [in/modified]
/// @return Pointer to the first node in the optimal path
///
SGraphNode * 
CMultiAligner::x_FindBestPath(vector<SGraphNode>& nodes)
{
    // Solve the graph problem detailed in x_FillPairInfo
    // below. nodes[] contains the alignments from which a 
    // subset will be chosen, and nodes[i] also contains a 
    // list of alignments that lie strictly to the right of 
    // alignment i.
    //
    // The algorithm is a simplified form of breadth-first
    // search, which relies on the list of alignments being
    // in sorted order. Optimal paths are built right-to-left,
    // from the rightmost alignment to the leftmost. For each
    // node, optimal paths for all nodes to the right have
    // already been computed, and the task reduces to determining
    // which path to attach the present node to, in order to
    // maximize the score of paths starting at the present node.
    //
    // Each node stores the best score for the path starting at
    // that node, and each node lies on at most one optimal path. 
    // The running time is O(number of edges in graph).
    //
    // This routine returns a linked list of structures that
    // give the alignments participating in the optimal path.
    // The 'best_score' field in the first linked list entry
    // gives the score of the optimal path.

    SGraphNode *best_node = NULL;
    int num_nodes = nodes.size();
    double best_score = INT4_MIN;

    // walk backwards through the list of nodes

    for (int i = num_nodes - 1; i >= 0; i--) {

        // walk through the list of edges for this node. Find
        // the node that is connected to the current node and
        // whose score is highest, then attach the current node
        // to it. Update the optimal score for the path that 
        // includes the current node

        CHit *hit1 = nodes[i].hit;
        double self_score = nodes[i].best_score;
        for (int j = i + 1; j < num_nodes; j++) {

            CHit *hit2 = nodes[j].hit;
            if (hit1->m_SeqRange1.StrictlyBelow(hit2->m_SeqRange1) &&
                hit1->m_SeqRange2.StrictlyBelow(hit2->m_SeqRange2) &&
                nodes[j].best_score + self_score > nodes[i].best_score) {

                nodes[i].best_score = nodes[j].best_score + self_score;
                nodes[i].path_next = &(nodes[j]);
            }
        }

        // the current node has been processed. See if a new global
        // optimum score has been reached

        if (nodes[i].best_score > best_score) {
            best_score = nodes[i].best_score;
            best_node = &(nodes[i]);
        }
    }

    return best_node;
}

/// Find a nonintersecting subset of the pairwise alignments available
/// between a collection of input sequences, and turn the alignments
/// into dynamic programming constraints
/// @param query_data The input sequences [in]
/// @param pair_info 2-D array of pairwise information [in/modified]
/// @param align_list The collection of pairwise alignments [in]
/// 
void 
CMultiAligner::x_FindAlignmentSubsets()
{
    // align_list contains all of the alignments found by blast
    // searches among all sequences, with the scores of these
    // alignments scaled by their frequency of occurence.
    //
    // This routine, for each pair of sequences, chooses a subset
    // of the available alignments for which 1) all alignments
    // occur in order, without crossing, on both sequences; and
    // 2) the combined score of all alignments is as large as possible.

    int num_queries = m_QueryData.size();
    CNcbiMatrix< vector<SGraphNode> > nodes(num_queries, num_queries);

    for (int i = 0; i < m_CombinedHits.Size(); i++) {
        m_CombinedHits.SetKeepHit(i, false);

        CHit *hit = m_CombinedHits.GetHit(i);
        nodes(hit->m_SeqIndex1, hit->m_SeqIndex2).push_back(SGraphNode(hit, i));
        nodes(hit->m_SeqIndex1, hit->m_SeqIndex2).back().best_score =
                                      hit->m_BitScore * hit->m_HitRate;
    }

    for (int i = 0; i < num_queries - 1; i++) {

        for (int j = i + 1; j < num_queries; j++) {

            // Find the optimal segment collection for this pair of queries

            SGraphNode *best_path = x_FindBestPath(nodes(i, j));

            // signal that the optimal path nodes should not be pruned

            while (best_path != NULL) {
                m_CombinedHits.SetKeepHit(best_path->list_pos, true);
                best_path = best_path->path_next;
            }
        }
    }

    m_CombinedHits.PurgeUnwantedHits();
}

void
CMultiAligner::x_FindConsistentHitSubset()
{
    if (!m_CombinedHits.Empty())
        m_CombinedHits.PurgeAllHits();

    m_CombinedHits += m_DomainHits;
    m_CombinedHits += m_LocalHits;
    m_CombinedHits.MakeCanonical();

    for (int i = 0; i < m_CombinedHits.Size(); i++) {
        CHit *hit = m_CombinedHits.GetHit(i);
        hit->m_BitScore = (hit->m_Score * m_KarlinBlk->Lambda -
                         m_KarlinBlk->logK) / NCBIMATH_LN2;
    }
    for (int i = 0; i < m_PatternHits.Size(); i++) {
        CHit *hit = m_PatternHits.GetHit(i);
        hit->m_BitScore = 1000.0;
    }
    for (int i = 0; i < m_UserHits.Size(); i++) {
        CHit *hit = m_UserHits.GetHit(i);
        hit->m_BitScore = 10000.0;
    }

    //---------------------------------------
    if (m_Verbose) {
        printf("\n\nAlignments before graph phase:\n");
        for (int i = 0; i < m_CombinedHits.Size(); i++) {
            CHit *hit = m_CombinedHits.GetHit(i);
            printf("query %2d %3d - %3d query %2d %3d - %3d score %lf (%lf)\n",
                   hit->m_SeqIndex1,
                   hit->m_SeqRange1.GetFrom(), hit->m_SeqRange1.GetTo(),
                   hit->m_SeqIndex2,
                   hit->m_SeqRange2.GetFrom(), hit->m_SeqRange2.GetTo(),
                   hit->m_BitScore, hit->m_BitScore * hit->m_HitRate);
        }
        printf("\n\n");
    }
    //---------------------------------------

    // weight each alignment found by the number of sequences
    // that contain that alignment

    // x_AssignHitRate();

    // For each pair of queries, find a maximal-scoring subset
    // of nonoverlapping alignments

    x_FindAlignmentSubsets();

    //---------------------------------------
    if (m_Verbose) {
        printf("\n\nAlignments after graph phase:\n");
        for (int i = 0; i < m_CombinedHits.Size(); i++) {
            CHit *hit = m_CombinedHits.GetHit(i);
            printf("query %2d %3d - %3d query %2d %3d - %3d score %lf (%lf)\n",
                   hit->m_SeqIndex1,
                   hit->m_SeqRange1.GetFrom(), hit->m_SeqRange1.GetTo(),
                   hit->m_SeqIndex2,
                   hit->m_SeqRange2.GetFrom(), hit->m_SeqRange2.GetTo(),
                   hit->m_BitScore, hit->m_BitScore * hit->m_HitRate);
        }
        printf("\n\n");
    }
    //---------------------------------------

    // weight each surviving alignment by the number of sequences
    // that contain that alignment

    // x_AssignHitRate();

    //--------------------------------------------------
    if (m_Verbose) {
        printf("Saved Segments:\n");
        for (int i = 0; i < m_CombinedHits.Size(); i++) {
            CHit *hit = m_CombinedHits.GetHit(i);
            printf("query %2d %3d - %3d query %2d %3d - %3d score %lf (%lf) ",
                   hit->m_SeqIndex1,
                   hit->m_SeqRange1.GetFrom(), hit->m_SeqRange1.GetTo(),
                   hit->m_SeqIndex2,
                   hit->m_SeqRange2.GetFrom(), hit->m_SeqRange2.GetTo(),
                   hit->m_BitScore, 
                   hit->m_BitScore * hit->m_HitRate);
            printf("\n");
        }
        printf("\n\n");
    }
    //--------------------------------------------------

}

END_SCOPE(cobalt)
END_NCBI_SCOPE

/*--------------------------------------------------------------------
  $Log$
  Revision 1.2  2005/11/07 20:44:34  papadopo
  1. Fix rcsid
  2. Fix incorrect scaling of hit rates

  Revision 1.1  2005/11/07 18:14:00  papadopo
  Initial revision

--------------------------------------------------------------------*/
