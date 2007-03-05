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

/// Find a maximum weight path in a directed acyclic graph
/// @param nodes The graph [in/modified]
/// @return Pointer to the first node in the optimal path
///
CMultiAligner::SGraphNode * 
CMultiAligner::x_FindBestPath(vector<SGraphNode>& nodes)
{
    // Given the nodes of a directed acyclic graph, each
    // contining a score and referring to a pairwise 
    // alignment between two sequences, find the non-
    // conflicting subset of the nodes that has the highest
    // score. Note that the scores optimized here are
    // local to the graph, and need not depend on the actual
    // alignment to which a node points.
    //
    // The algorithm is a simplified form of dynamic programming,
    // which relies on the list of alignments being
    // in sorted order. Optimal paths are built right-to-left,
    // from the rightmost alignment to the leftmost. For each
    // node, optimal paths for all nodes to the right have
    // already been computed, and the task reduces to determining
    // which path to attach the present node to, in order to
    // maximize the score of paths starting at the present node.
    //
    // Each node stores the best score for the path starting at
    // that node, and each node lies on at most one optimal path. 
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

        // Find the node that lies strictly to the right of
        // of node i and is the head of the currently highest-scoring
        // collection of nodes. Then attach the current node
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

void 
CMultiAligner::x_FindAlignmentSubsets()
{
    // For each pair of sequences, chooses a subset of the 
    // available alignments for which 1) all alignments
    // occur in order, without crossing, on both sequences; and
    // 2) the combined score of all alignments is as large as possible.
    // The following relies on m_CombinedList being sorted in
    // canonical order

    int num_queries = m_QueryData.size();
    CNcbiMatrix< vector<SGraphNode> > nodes(num_queries, num_queries);

    // first group together alignments to the same pairs of
    // sequences. Each such collection of alignments becomes
    // a node in the graph for that sequence pair; the score
    // of the node is the score of the hit it points to

    for (int i = 0; i < m_CombinedHits.Size(); i++) {

        // assume all hits will be deleted

        m_CombinedHits.SetKeepHit(i, false);

        CHit *hit = m_CombinedHits.GetHit(i);
        nodes(hit->m_SeqIndex1, hit->m_SeqIndex2).push_back(SGraphNode(hit, i));
        nodes(hit->m_SeqIndex1, hit->m_SeqIndex2).back().best_score =
                                      hit->m_Score;
    }

    // now optimize the graph for each sequence pair
    // separately

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

    // Remove hits that do not lie on an optimal path

    m_CombinedHits.PurgeUnwantedHits();
}

void
CMultiAligner::x_FindConsistentHitSubset()
{
    m_CombinedHits.MakeCanonical();

    // For each pair of queries, find a maximal-scoring subset
    // of nonoverlapping alignments

    x_FindAlignmentSubsets();

    //--------------------------------------------------
    if (m_Verbose) {
        printf("Saved Segments:\n");
        for (int i = 0; i < m_CombinedHits.Size(); i++) {
            CHit *hit = m_CombinedHits.GetHit(i);
            printf("query %2d %3d - %3d query %2d %3d - %3d score %d\n",
                   hit->m_SeqIndex1,
                   hit->m_SeqRange1.GetFrom(), hit->m_SeqRange1.GetTo(),
                   hit->m_SeqIndex2,
                   hit->m_SeqRange2.GetFrom(), hit->m_SeqRange2.GetTo(),
                   hit->m_Score);
        }
        printf("\n\n");
    }
    //--------------------------------------------------

}

END_SCOPE(cobalt)
END_NCBI_SCOPE
