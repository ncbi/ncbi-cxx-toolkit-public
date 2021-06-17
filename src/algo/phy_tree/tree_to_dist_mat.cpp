/*  $Id$
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
 * Authors:  Josh Cherry
 *
 * File Description:  Create a distance matrix from a phylogenetic tree
 *
 */


#include <ncbi_pch.hpp>
#include <algo/phy_tree/tree_to_dist_mat.hpp>

BEGIN_NCBI_SCOPE

// Recursive function that does most of the work
static void s_NodeToDistMat(const CBioTreeDynamic::CBioNode& node,
                            CNcbiMatrix<double>& mat,
                            vector<string>& labels,
                            vector<double>& leaf_distances,
                            const string& label_feature,
                            const string& dist_feature)
{
    if (node.IsLeaf()) {
        // Base case
        labels.push_back(node[label_feature]);
        leaf_distances.push_back(0);
        return;
    }
    CBioTreeDynamic::CBioNode::TNodeList_CI it = node.SubNodeBegin();
    for (;  it != node.SubNodeEnd();  ++it) {
        const CBioTreeDynamic::CBioNode& sub_node =
            static_cast<const CBioTreeDynamic::CBioNode&>(**it);
        unsigned int start = labels.size();

        // Recursion
        s_NodeToDistMat(sub_node, mat, labels, leaf_distances, 
                        label_feature, dist_feature);

        double sub_node_dist;
        if (dist_feature != "") {
            sub_node_dist = NStr::StringToDouble(sub_node[dist_feature]);
        } else {
            sub_node_dist = 1;
        }
        for (unsigned int i = start; i < leaf_distances.size(); ++i) {
            leaf_distances[i] += sub_node_dist;
        }
        for (unsigned int i = 0; i < start; ++i) {
            for (unsigned int j = start; j < labels.size(); ++j) {
                mat(i, j) = mat(j, i) = leaf_distances[i] + leaf_distances[j];
            }
        }
    }
}


// Recursive function for counting the number of leaf nodes
static unsigned int s_CountLeaves(const CBioTreeDynamic::CBioNode& node)
{
    if (node.IsLeaf()) {
        return 1;
    }
    unsigned int count = 0;
    CBioTreeDynamic::CBioNode::TNodeList_CI it = node.SubNodeBegin();
    for (;  it != node.SubNodeEnd();  ++it) {
        const CBioTreeDynamic::CBioNode& sub_node =
            static_cast<const CBioTreeDynamic::CBioNode&>(**it);
        count += s_CountLeaves(sub_node);
    }
    return count;
}


// Create a distance matrix and list of labels from a tree node
void NodeToDistMat(const CBioTreeDynamic::CBioNode& node,
                   CNcbiMatrix<double>& mat,
                   vector<string>& labels,
                   const string& label_feature,
                   const string& dist_feature)
{

    vector<double> leaf_distances;
    unsigned int leaf_count = s_CountLeaves(node);
    mat.Resize(0, 0);
    mat.Resize(leaf_count, leaf_count);
    leaf_distances.reserve(leaf_count);
    labels.clear();
    labels.reserve(leaf_count);
    s_NodeToDistMat(node, mat, labels, leaf_distances,
                    label_feature, dist_feature);
}


// Create a distance matrix and list of labels from a tree
void TreeToDistMat(const CBioTreeDynamic& tree,
                   CNcbiMatrix<double>& mat,
                   vector<string>& labels,
                   const string& label_feature,
                   const string& dist_feature)
{
    NodeToDistMat(*tree.GetTreeNode(), mat, labels,
                  label_feature, dist_feature);
}


END_NCBI_SCOPE
