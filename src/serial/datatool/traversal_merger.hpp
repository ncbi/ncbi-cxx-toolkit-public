#ifndef TRAVERSALMERGER__HPP
#define TRAVERSALMERGER__HPP

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
* Author: Michael Kornbluh
*
* File Description:
*   Tries to merge user functions that are identical.
*/

#include "traversal_node.hpp"

BEGIN_NCBI_SCOPE

class CTraversalMerger {
public:
    // The constructor does all the work
    CTraversalMerger( 
        const CTraversalNode::TNodeVec &rootTraversalNodes,
        const CTraversalNode::TNodeSet &nodesWithFunctions );

private:

    // Gives a unique string for a node such
    // that two nodes get the same string iff they
    // are mergeable.  This function does cover cases 
    // where there are cycles.
    class CNodeLabeler {
    public:
        enum ERootEncountered {
            eRootEncountered_No = 1,
            eRootEncountered_Yes
        };

        // all work done in the constructor
        CNodeLabeler(CRef<CTraversalNode> start_node, string &out_result, 
            ERootEncountered &out_root_encountered,
            const CTraversalNode::TNodeSet &root_nodes );
    private:
        enum EIsCyclic {
            eIsCyclic_NonCyclic = 1,
            eIsCyclic_Cyclic
        };

        void x_AppendNodeLabelRecursively( CRef<CTraversalNode> node );

        // non-recursive.  Just gives the plain label
        EIsCyclic x_AppendDirectNodeLabel( CRef<CTraversalNode> node );

        typedef std::map< CRef<CTraversalNode>, int > TNodeToIntMap;
        TNodeToIntMap m_NodeToIntMap;
        int m_PrevNodeInt;

        // When a usercall refers to another node, we check that
        // that node is somewhere in the nodes we come across.
        // If there is a dependency outside of that, the node
        // is unmergeable.
        set< CRef<CTraversalNode> > m_DependencyNodes;

        // these are member variables so we don't have to pass
        // them as part of the recursion
        string &m_Out_result;
        ERootEncountered &m_Out_root_encountered;
        const CTraversalNode::TNodeSet & m_Root_nodes;

        // each unmergeable node we find gets a unique number
        static int ms_NumUnmergeableSoFar;
    };

    // process current tier of nodes and load up next tier
    // (for partial breadth-first search)
    void x_DoTier( 
        const CTraversalNode::TNodeSet &currentTier, 
        CTraversalNode::TNodeSet &nextTier );

    void x_AddCallersToTier( 
        CRef<CTraversalNode> current_node, 
        CTraversalNode::TNodeSet &tier );

    // to prevent infinite loops
    CTraversalNode::TNodeSet m_NodesSeen;

    const CTraversalNode::TNodeSet m_RootTraversalNodes;
};

END_NCBI_SCOPE

#endif /* TRAVERSALMERGER__HPP */
