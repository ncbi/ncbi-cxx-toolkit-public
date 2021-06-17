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

File name: tree.cpp

Author: Jason Papadopoulos

Contents: Implementation of CTree class

******************************************************************************/

/// @file tree.cpp
/// Implementation of CTree class

#include <ncbi_pch.hpp>
#include <algo/cobalt/tree.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

void 
CTree::PrintTree(const TPhyTreeNode *tree, int level)
{
    int i, j;

    for (i = 0; i < level; i++)
        printf("    ");

    printf("node: ");
    if (tree->IsLeaf() && tree->GetValue().GetId() >= 0)
        printf("query %d ", tree->GetValue().GetId());
    if (tree->GetValue().IsSetDist())
        printf("distance %lf", tree->GetValue().GetDist());
    printf("\n");

    if (tree->IsLeaf())
        return;

    TPhyTreeNode::TNodeList_CI child(tree->SubNodeBegin());

    j = 0;
    while (child != tree->SubNodeEnd()) {
        for (i = 0; i < level; i++)
            printf("    ");

        printf("%d:\n", j);
        PrintTree(*child, level + 1);

        j++;
        child++;
    }
}

void
CTree::ListTreeEdges(const TPhyTreeNode *node, 
                     vector<STreeEdge>& edge_list, int max_id)
{
    if (node->GetParent()) {
        edge_list.push_back(STreeEdge(node, node->GetValue().GetDist()));
    }
    // check whether to traverse the tree below this node
    if (max_id >= 0 && node->GetValue().GetId() >= max_id) {
        return;
    }

    TPhyTreeNode::TNodeList_CI child(node->SubNodeBegin());
    while (child != node->SubNodeEnd()) {
        ListTreeEdges(*child, edge_list, max_id);
        child++;
    }
}

void
CTree::ListTreeLeaves(const TPhyTreeNode *node, 
            vector<STreeLeaf>& node_list,
            double curr_dist)
{
    if (node->IsLeaf()) {

        // negative edge lengths are not valid, and we assume
        // that distances closer to the root are weighted more
        // heavily, hence the reciprocal

        if (curr_dist <= 0)
            node_list.push_back(STreeLeaf(node->GetValue().GetId(), 0.0));
        else
            node_list.push_back(STreeLeaf(node->GetValue().GetId(),
                                          1.0 / curr_dist));
        return;
    }

    TPhyTreeNode::TNodeList_CI child(node->SubNodeBegin());
    while (child != node->SubNodeEnd()) {
        double new_dist = curr_dist;

        // the fastme algorithm does not allow edges to have
        // negative length unless they are leaf edges. We allow
        // lengths that are slightly negative to account for
        // roundoff effects

        _ASSERT((*child)->GetValue().GetDist() >= -1e-6 || (*child)->IsLeaf());
        if ((*child)->GetValue().GetDist() > 0) {
            new_dist += (*child)->GetValue().GetDist();
        }

        ListTreeLeaves(*child, node_list, new_dist);
        child++;
    }
}

void
CTree::ComputeTree(const CDistMethods::TMatrix& distances,
                   bool use_fastme)
{
    if (m_Tree) {
        delete m_Tree;
    }

    if (use_fastme)
        m_Tree = CDistMethods::FastMeTree(distances);
    else
        m_Tree = CDistMethods::NjTree(distances);

    m_Tree->GetValue().SetDist(0.0);

    m_Tree = CDistMethods::RerootTree(m_Tree);
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
