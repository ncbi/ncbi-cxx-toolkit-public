/* $Id$
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

File name: tree.hpp

Author: Jason Papadopoulos

Contents: Interface for CTree class

******************************************************************************/

/// @file tree.hpp
/// Interface for CTree class

#ifndef _ALGO_COBALT_TREE_HPP_
#define _ALGO_COBALT_TREE_HPP_

#include <algo/cobalt/base.hpp>
#include <algo/cobalt/seq.hpp>
#include <algo/cobalt/hitlist.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

class CTree {

public:

    typedef struct STreeEdge {
        const TPhyTreeNode *node;
        double distance;

        STreeEdge(const TPhyTreeNode *n, double d)
            : node(n), distance(d) {}
    } STreeEdge;
    
    typedef struct STreeLeaf {
        int query_idx;
        double distance;

        STreeLeaf(int q, double d)
            : query_idx(q), distance(d) {}
    } STreeLeaf;

    CTree() { m_Tree = 0; }

    CTree(const CDistMethods::TMatrix& distances)
    {
        m_Tree = 0;
        ComputeTree(distances);
    }

    ~CTree() { delete m_Tree; }

    void ComputeTree(const CDistMethods::TMatrix& distances);

    const TPhyTreeNode * GetTree() const { return m_Tree; }

    static void ListTreeLeaves(const TPhyTreeNode *node,
                               vector<STreeLeaf>& node_list,
                               double curr_dist);
    static void ListTreeEdges(const TPhyTreeNode *node,
                              vector<STreeEdge>& edge_list);
    static void PrintTree(const TPhyTreeNode *node,
                          int level = 0);

private:
    TPhyTreeNode *m_Tree;

    void x_RerootTree(TPhyTreeNode *node);
    TPhyTreeNode *x_FindLargestEdge(TPhyTreeNode *node,
                                    TPhyTreeNode *best_node);
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // _ALGO_COBALT_TREE_HPP_

/*--------------------------------------------------------------------
  $Log$
  Revision 1.1  2005/11/07 18:15:52  papadopo
  Initial revision

--------------------------------------------------------------------*/
