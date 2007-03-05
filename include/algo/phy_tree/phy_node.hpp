#ifndef ALGO_PHY_TREE___PHY_NODE__HPP
#define ALGO_PHY_TREE___PHY_NODE__HPP

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
 * File Description:  Things for representing and manipulating
 *          phylogenetic trees
 *
 */


#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_tree.hpp>

BEGIN_NCBI_SCOPE

/// Data contained at each node of a phylogenetic tree.
///
/// Holds a label (string), a distance (double), and an integer id.
/// Distance may not be set; IsSetDist() reports whether it is.
/// id == -1 indicates that the id is not set.

class NCBI_XALGOPHYTREE_EXPORT CPhyNodeData
{
public:
    CPhyNodeData(void)
        : m_Id(-1), m_DistSet(false)
    {
    }

    int GetId(void) const {return m_Id;}

    void SetId(int id) {m_Id = id;}

    /// Is the distance set?
    bool IsSetDist(void) const {return m_DistSet;}

    /// Return the distance; does NOT check whether it's really set.
    double GetDist(void) const {return m_Dist;}
    void SetDist(double dist) {m_Dist = dist; m_DistSet = true;}

    /// Make it no longer set.
    void ResetDist(void) {m_DistSet = false; m_Dist = 0;}
    const string& GetLabel(void) const {return m_Label;}
    void SetLabel(const string& label) {m_Label = label;}
    string& SetLabel(void) {return m_Label;}

private:
    int m_Id;
    double m_Dist;
    bool m_DistSet;
    string m_Label;
};


typedef CTreeNode<CPhyNodeData> TPhyTreeNode;

/// Newick format output
NCBI_XALGOPHYTREE_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& os, const TPhyTreeNode& tree);

/// Nexus format output (Newick with some stuff around it).
///
/// tree_name gets put in the file.
NCBI_XALGOPHYTREE_EXPORT
void WriteNexusTree(CNcbiOstream& os, const TPhyTreeNode& tree,
                    const string& tree_name = "the_tree");

/// Newick but without the terminal ';'
NCBI_XALGOPHYTREE_EXPORT
void PrintNode(CNcbiOstream& os, const TPhyTreeNode& node);

/// Newick format input.
///
/// Uses flex/bison lexer/parser.
NCBI_XALGOPHYTREE_EXPORT
TPhyTreeNode *ReadNewickTree(CNcbiIstream& is);

END_NCBI_SCOPE

#endif  // ALGO_PHY_TREE___PHY_NODE__HPP
