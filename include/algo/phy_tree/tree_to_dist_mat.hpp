#ifndef ALGO_PHY_TREE___TREE_TO_DIST_MAT__HPP
#define ALGO_PHY_TREE___TREE_TO_DIST_MAT__HPP

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

#include <corelib/ncbistd.hpp>
#include <algo/phy_tree/bio_tree.hpp>
#include <util/math/matrix.hpp>

BEGIN_NCBI_SCOPE

/// Create a distance matrix and list of labels from a tree.
/// Distances are the pairwise distances implied by the tree.
/// The "label_feature" parameter specifies the field to
/// turn into labels.  The "dist_feature" parameter specifies
/// the field to use for branch length.  If "dist_feature" is
/// the empty string (""), take all branch lengths to be 1
/// (i.e., count the branches).
NCBI_XALGOPHYTREE_EXPORT
void TreeToDistMat(const CBioTreeDynamic& tree,
                   CNcbiMatrix<double>& mat,
                   vector<string>& labels,
                   const string& label_feature = "label",
                   const string& dist_feature = "dist");

/// Create a distance matrix and list of labels from a tree node
/// and its descendants.  See TreeToDist for more information.
NCBI_XALGOPHYTREE_EXPORT
void NodeToDistMat(const CBioTreeDynamic::CBioNode& node,
                   CNcbiMatrix<double>& mat,
                   vector<string>& labels,
                   const string& label_feature = "label",
                   const string& dist_feature = "dist");

END_NCBI_SCOPE

#endif  // ALGO_PHY_TREE___TREE_TO_DIST_MAT__HPP
