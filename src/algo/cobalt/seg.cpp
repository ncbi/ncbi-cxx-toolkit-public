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

/// Find a maximum weight path in a directed acyclic graph
/// @param nodes The graph [in/modified]
/// @return Pointer to the first node in the optimal path
///
CMultiAligner::SGraphNode * 
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
                                      hit->m_Score;
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
    if (m_CombinedHits.Size() < 2)
        return;

    m_CombinedHits.MakeCanonical();

    //---------------------------------------
    if (m_Verbose) {
        printf("\n\nAlignments before graph phase:\n");
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
    //---------------------------------------

    // For each pair of queries, find a maximal-scoring subset
    // of nonoverlapping alignments

    x_FindAlignmentSubsets();

    //---------------------------------------
    if (m_Verbose) {
        printf("\n\nAlignments after graph phase:\n");
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
    //---------------------------------------

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

/*--------------------------------------------------------------------
  $Log$
  Revision 1.8  2005/11/18 20:21:30  papadopo
  1. Use raw score instead of bit scores in the graph phase
  2. Remove unneeded comments

  Revision 1.7  2005/11/17 22:31:22  papadopo
  remove hit rate computations

  Revision 1.6  2005/11/10 16:18:32  papadopo
  Allow hitlists to be regenerated cleanly

  Revision 1.5  2005/11/10 15:39:54  papadopo
  SGraphNode is now private to CMultiAligner

  Revision 1.4  2005/11/08 18:42:16  papadopo
  assert -> _ASSERT

  Revision 1.3  2005/11/08 17:55:25  papadopo
  ASSERT -> assert

  Revision 1.2  2005/11/07 20:44:34  papadopo
  1. Fix rcsid
  2. Fix incorrect scaling of hit rates

  Revision 1.1  2005/11/07 18:14:00  papadopo
  Initial revision

--------------------------------------------------------------------*/
