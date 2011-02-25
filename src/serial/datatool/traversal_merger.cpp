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

#include <ncbi_pch.hpp>
#include "traversal_merger.hpp"
#include <iterator>

BEGIN_NCBI_SCOPE

CTraversalMerger::CTraversalMerger( 
        const CTraversalNode::TNodeVec &rootTraversalNodes,
        const CTraversalNode::TNodeSet &nodesWithFunctions )
        : m_RootTraversalNodes(rootTraversalNodes),
          m_NodesSeen( rootTraversalNodes.begin(), rootTraversalNodes.end() ) // so we don't process root nodes
{
    // we do a partial breadth-first search upward from the nodes with functions, merging nodes wherever we can
    CTraversalNode::TNodeSet currentTier;
    CTraversalNode::TNodeSet nextTier;

    // The only nodes that might conceivably be mergeable at the beginning 
    // are those with functions but no callees.
    ITERATE( CTraversalNode::TNodeSet, node_iter, nodesWithFunctions ) {
        if( (*node_iter)->GetCallees().empty() ) {
            currentTier.insert( *node_iter );
        }
    }

    while( ! currentTier.empty() ) {
        x_DoTier( currentTier, nextTier );

        // switch to next tier
        nextTier.swap( currentTier );
        nextTier.clear();
    }
}

template< typename TIter1, typename TIter2, typename TComparator >
static int s_Lexicographical_compare_arrays( 
    TIter1 begin1, TIter1 end1, 
    TIter2 begin2, TIter2 end2, 
    TComparator comparator )
{
    TIter1 iter1 = begin1;
    TIter2 iter2 = begin2;

    for( ; iter1 != end1 && iter2 != end2 ; ++iter1, ++iter2 ) {
        const int comparison = comparator( *iter1, *iter2 );
        if( comparison != 0 ) {
            return comparison;
        }
    }

    // shorter sequence first
    if( iter1 == end1 ) {
        if( iter2 == end2 ) {
            return 0;
        } else {
            return -1;
        }
    } else if( iter2 == end2 ) {
        return 1;
    }
    _ASSERT(false); // should be impossible to reach here
    return 0;
}

class CCompareCRefUserCall
{
public:
    int operator()( 
        const CRef<CTraversalNode::CUserCall> &call1, 
        const CRef<CTraversalNode::CUserCall> &call2 )
    {
        return call1->Compare( *call2 );
    }
};

bool
CTraversalMerger::CMergeLessThan::operator()( 
            const CRef<CTraversalNode> &node1, 
            const CRef<CTraversalNode> &node2 ) const
{
    // CTraversalNodes are the same if they have the same input type, type,
    // var name and children

    // special case, nodes that contain user calls that reference other nodes
    // can't be merged.
    const bool node1_is_unmergeable = ! x_IsNodeMergeable( node1 );
    const bool node2_is_unmergeable = ! x_IsNodeMergeable( node2 );
    if( node1_is_unmergeable != node2_is_unmergeable ) {
        return ( (int)node1_is_unmergeable < (int)node2_is_unmergeable );
    }
    if( node1_is_unmergeable ) { // since same, node2_is_unmergeable is also true
        return node1 < node2; // does pointer comparison
    }

    // compare input class
    const int class_comparison = 
        NStr::Compare( node1->GetInputClassName(), node2->GetInputClassName() );
    if( class_comparison != 0 ) {
        return ( class_comparison < 0 );
    }

    // compare type
    int type_comparison = ( node1->GetType() - node2->GetType() );
    if( type_comparison != 0 ) {
        return (type_comparison < 0);
    }

    // compare var name
    const int var_comparison = NStr::Compare( node1->GetVarName(), node2->GetVarName() );
    if( var_comparison != 0 ) {
        return (var_comparison < 0);
    }

    // compare user calls (pre)
    const int pre_callee_compare = s_Lexicographical_compare_arrays( 
        node1->GetPreCalleesUserCalls().begin(), node1->GetPreCalleesUserCalls().end(), 
        node2->GetPreCalleesUserCalls().begin(), node2->GetPreCalleesUserCalls().end(), 
        CCompareCRefUserCall() );
    if( pre_callee_compare != 0 ) {
        return ( pre_callee_compare < 0 );
    }

    // compare user calls (post)
    const int post_callee_compare = s_Lexicographical_compare_arrays( 
        node1->GetPostCalleesUserCalls().begin(), node1->GetPostCalleesUserCalls().end(), 
        node2->GetPostCalleesUserCalls().begin(), node2->GetPostCalleesUserCalls().end(), 
        CCompareCRefUserCall() );
    if( post_callee_compare != 0 ) {
        return ( post_callee_compare < 0 );
    }

    // compare children
    return ( node1->GetCallees() < node2->GetCallees() );
}

bool CTraversalMerger::CMergeLessThan::x_IsNodeMergeable( const CRef<CTraversalNode> &node ) const
{
    // If the node has any user calls that rely on other nodes, it's unmergeable.
    // Imagine that A calls B calls C, and also D calls E calls F.
    // Let's say that C uses A's arg.  You can't merge C and F
    // because then the new node might be called via D, so C's arg is never
    // filled in and you crash on a NULL-pointer dereference.
    ITERATE( CTraversalNode::TUserCallVec, call_iter, node->GetPreCalleesUserCalls() ) {
        if( ! (*call_iter)->GetExtraArgNodes().empty() ) {
            return false;
        }
    }
    ITERATE( CTraversalNode::TUserCallVec, a_call_iter, node->GetPostCalleesUserCalls() ) {
        if( ! (*a_call_iter)->GetExtraArgNodes().empty() ) {
            return false;
        }
    }

    return true;
}

void
CTraversalMerger::x_DoTier( 
    const CTraversalNode::TNodeSet &currentTier, 
    CTraversalNode::TNodeSet &nextTier )
{
    _ASSERT( nextTier.empty() );
    // see which nodes in the current tier can be merged together and 
    // set the callers of any merged nodes to be the nextTier, since
    // they might become mergeable.

    // CMergeLessThan considers nodes equal when they're mergeable
    typedef map< CRef<CTraversalNode>,  CTraversalNode::TNodeVec, CMergeLessThan > TMergeMap;

    TMergeMap merge_map;
    ITERATE( CTraversalNode::TNodeSet, node_iter, currentTier ) {
        merge_map[*node_iter].push_back( *node_iter );
    }
    
    // for each mergeable set of nodes, merge them and place their callers on the
    // next tier
    ITERATE( TMergeMap, merge_iter, merge_map ) {
        const CTraversalNode::TNodeVec &merge_vec = merge_iter->second;
        if( merge_vec.size() > 1 ) {
            // merge all nodes into the node with the shortest func name
            CTraversalNode::TNodeVec::const_iterator do_merge_iter =
                merge_vec.begin();
            CRef<CTraversalNode> target = *do_merge_iter;
            for( ; do_merge_iter != merge_vec.end(); ++do_merge_iter ) {
                if( (*do_merge_iter)->GetFuncName().length() < target->GetFuncName().length() ) {
                    target = *do_merge_iter;
                }
            }

            do_merge_iter = merge_vec.begin();
            for( ; do_merge_iter != merge_vec.end(); ++do_merge_iter ) {
                target->Merge( *do_merge_iter );
            }
            const CTraversalNode::TNodeSet &callers = target->GetCallers();
            copy( callers.begin(), callers.end(), inserter(nextTier, nextTier.begin()) );
        }
        // TODO: would it help to be able to merge into root nodes?
    }
}

END_NCBI_SCOPE
