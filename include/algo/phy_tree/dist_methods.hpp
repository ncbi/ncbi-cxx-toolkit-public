#ifndef ALGO_PHY_TREE___DIST_METHODS__HPP
#define ALGO_PHY_TREE___DIST_METHODS__HPP

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
 * File Description:  Distance methods for phylogenic tree reconstruction
 *
 */


#include <objtools/alnmgr/alnvec.hpp>
#include <util/creaders/alignment_file.hpp>
#include <util/math/matrix.hpp>
#include <algo/phy_tree/phy_node.hpp>

// these are for conversion to CBioTreeContainer
#include <objects/biotree/BioTreeContainer.hpp>
#include <objects/biotree/FeatureDescr.hpp>
#include <objects/biotree/Node.hpp>
#include <objects/biotree/NodeSet.hpp>
#include <objects/biotree/NodeFeature.hpp>
#include <objects/biotree/NodeFeatureSet.hpp>
#include <objects/biotree/FeatureDictSet.hpp>

BEGIN_NCBI_SCOPE


class NCBI_XALGOPHYTREE_EXPORT CDistMethods
{
public:
    typedef CNcbiMatrix<double> TMatrix;
    typedef TPhyTreeNode TTree;

    enum EFastMePar {
        eOls = 1,
        eBalanced = 2
    };

    /// Jukes-Cantor distance calculation for DNA sequences:
    /// d = -3/4 ln(1 - (4/3)p).
    static void JukesCantorDist(const TMatrix& frac_diff,
                                TMatrix& result);

    /// Simple distance calculation for protein sequences:
    /// d = -ln(1 - p).
    static void PoissonDist(const TMatrix& frac_diff,
                            TMatrix& result);

    /// Kimura's distance for protein sequences:
    /// d = -ln(1 - p - 0.2p^2).
    static void KimuraDist(const TMatrix& frac_diff,
                           TMatrix& result);

    /// Calculate pairwise fractions of non-identity
    static double Divergence(const string& seq1, const string& seq2);
    static void Divergence(const objects::CAlnVec& avec_in, TMatrix& result);
    //static void Divergence(const CAlignment& aln, TMatrix& result);

    /// Compute a tree by neighbor joining; 
    /// as per Hillis et al. (Ed.), Molecular Systematics, pg. 488-489.
    static TTree *NjTree(const TMatrix& dist_mat,
                         const vector<string>& labels = vector<string>());

    /// Compute a tree using the fast minimum evolution algorithm
    static TTree *FastMeTree(const TMatrix& dist_mat,
                             const vector<string>& labels = vector<string>(),
                             EFastMePar btype = eOls,
                             EFastMePar wtype = eOls,
                             EFastMePar ntype = eBalanced);

    /// Check a matrix for NaNs and Infs
    static bool AllFinite(const TMatrix& mat);
};

/// Conversion from TPhyTreeNode to CBioTreeContainer
NCBI_XALGOPHYTREE_EXPORT CRef<objects::CBioTreeContainer>
MakeBioTreeContainer(const TPhyTreeNode *tree);

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/02/16 15:42:55  jcherry
 * Made tree-building methods throw if distance matrix contains
 * NaNs or Infs.  Added CDistMethods::AllFinite to check this.
 *
 * Revision 1.7  2004/07/01 21:11:23  jcherry
 * Added export specifier
 *
 * Revision 1.6  2004/07/01 19:44:53  jcherry
 * Added function for making CBioTreeContainer from TPhyTreeNode
 *
 * Revision 1.5  2004/02/19 16:43:46  jcherry
 * Temporarily disable one form of Divergence() method
 *
 * Revision 1.4  2004/02/19 13:21:58  dicuccio
 * Roll back to version 1.2
 *
 * Revision 1.2  2004/02/10 17:02:28  dicuccio
 * Formatting changes.  Added export specifiers
 *
 * Revision 1.1  2004/02/10 15:15:56  jcherry
 * Initial version
 *
 * ===========================================================================
 */


#endif  // ALGO_PHY_TREE___DIST_METHODS__HPP
