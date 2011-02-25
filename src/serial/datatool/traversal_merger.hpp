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
    // sorts CRef<CTraversalNode> objects, BUT they're equal
    // if they're mergeable
    class CMergeLessThan {
    public:
        bool operator()( 
            const CRef<CTraversalNode> &node1, 
            const CRef<CTraversalNode> &node2 ) const;

        bool x_IsNodeMergeable( const CRef<CTraversalNode> &node ) const;
    };

    // process current tier of nodes and load up next tier
    // (for partial breadth-first search)
    void x_DoTier( 
        const CTraversalNode::TNodeSet &currentTier, 
        CTraversalNode::TNodeSet &nextTier );

    // to prevent infinite loops
    CTraversalNode::TNodeSet m_NodesSeen;

    const CTraversalNode::TNodeVec &m_RootTraversalNodes;
};

END_NCBI_SCOPE

#endif /* TRAVERSALMERGER__HPP */