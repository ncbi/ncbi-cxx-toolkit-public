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
        : m_RootTraversalNodes( rootTraversalNodes.begin(), rootTraversalNodes.end() )
{
    // we do a partial breadth-first search upward from the nodes with functions, merging nodes wherever we can
    CTraversalNode::TNodeSet currentTier;
    CTraversalNode::TNodeSet nextTier;

    // Don't forget to consider cycles.
    // e.g. A -> B -> C -> A(again), but where
    // A, B and C can be merged into other nodes D, E and F
    // that have an identical cycle with the same types and
    // everything.
    ITERATE( CTraversalNode::TNodeSet, node_iter, nodesWithFunctions ) {
        currentTier.insert( *node_iter );
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

int CTraversalMerger::CNodeLabeler::ms_NumUnmergeableSoFar = 0;

CTraversalMerger::CNodeLabeler::CNodeLabeler( 
    ncbi::CRef<CTraversalNode> node, string &out_result, 
    ERootEncountered &out_root_encountered,
    const CTraversalNode::TNodeSet &root_nodes )
: m_PrevNodeInt(0), m_Out_result(out_result), 
    m_Out_root_encountered(out_root_encountered),
    m_Root_nodes(root_nodes)
{
    // clear args
    m_Out_result.clear();
    m_Out_root_encountered = eRootEncountered_No;

    // assign values to output variables
    x_AppendNodeLabelRecursively( node );

    // Before leaving, make sure all dependencies are met,
    // otherwise we mark the node as unmergeable
    ITERATE( set< CRef<CTraversalNode> >, dependency_iter, m_DependencyNodes ) {
        if( m_NodeToIntMap.find(*dependency_iter) == m_NodeToIntMap.end() ) {
            // There's a dependency outside of the nodes we encountered, so give
            // this node a unique label so it won't be merged into anything.
            m_Out_result = "(UNMERGEABLE" + NStr::IntToString(++ms_NumUnmergeableSoFar) + ")";
            break;
        }
    }
}

void 
CTraversalMerger::CNodeLabeler::x_AppendNodeLabelRecursively( CRef<CTraversalNode> node )
{
    if( eIsCyclic_NonCyclic == x_AppendDirectNodeLabel( node ) ) {
        // If non-cyclic call, we're free to traverse the children, if there are any
        if( ! node->GetCallees().empty() ) {
            m_Out_result += "{";
            ITERATE( CTraversalNode::TNodeCallSet, callee_iter, node->GetCallees() ) {
                x_AppendNodeLabelRecursively( (*callee_iter)->GetNode() );
                m_Out_result += ","; // we don't care if last one has unnecessary comma
            }
            m_Out_result += "}";
        }
    }
}

// non-recursive. Just gives the plain label
CTraversalMerger::CNodeLabeler::EIsCyclic
CTraversalMerger::CNodeLabeler::x_AppendDirectNodeLabel( CRef<CTraversalNode> node )
{
    // First check if the node already has an int.
    // If so, we just return that number as a string
    TNodeToIntMap::iterator node_location = m_NodeToIntMap.find( node );
    if( node_location != m_NodeToIntMap.end() ) {
        m_Out_result += NStr::IntToString( node_location->second );
        return eIsCyclic_Cyclic;
    }

    // Otherwise, we have to create the string ourselves:
    const int node_int = ++m_PrevNodeInt;
    m_Out_result += "(" + NStr::IntToString( node_int );
    m_NodeToIntMap[node] = node_int; // store the int for later use

    // name does NOT include fields that don't directly affect:
    // the function's behavior( e.g. funcname, callers, etc.)
    // The label doesn't include callees, but that's only
    // because it will be added recursively elsewhere

    m_Out_result += "[";
    ITERATE( CTraversalNode::TUserCallVec, pre_user_call_iter, node->GetPreCalleesUserCalls() ) {
        // we don't care if the last one has an unnecessary comma
        m_Out_result += (*pre_user_call_iter)->GetUserFuncName() + ",";
        const CTraversalNode::TNodeVec &node_vec = (*pre_user_call_iter)->GetExtraArgNodes();
        copy( node_vec.begin(), node_vec.end(), 
            inserter(m_DependencyNodes, m_DependencyNodes.begin()) );
    }
    m_Out_result += "][";
    ITERATE( CTraversalNode::TUserCallVec, post_user_call_iter, node->GetPostCalleesUserCalls() ) {
        // we don't care if the last one has an unnecessary comma
        m_Out_result += (*post_user_call_iter)->GetUserFuncName() + ",";
        const CTraversalNode::TNodeVec &node_vec = (*post_user_call_iter)->GetExtraArgNodes();
        copy( node_vec.begin(), node_vec.end(), 
            inserter(m_DependencyNodes, m_DependencyNodes.begin()) );
    }
    m_Out_result += "]";

    m_Out_result += node->GetTypeAsString() + ",";
    m_Out_result += node->GetInputClassName();

    m_Out_result += ")";

    // check if the given node is a root node
    if( m_Root_nodes.find(node) != m_Root_nodes.end() ) {
        m_Out_root_encountered = eRootEncountered_Yes;
    }

    return eIsCyclic_NonCyclic;
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

    // In the "pair", the bool is true iff the node calls a
    // root node somehow (directly or indirectly or *is* a root)
    typedef pair< bool, CRef<CTraversalNode> > TMergeMapPair;
    typedef vector<TMergeMapPair> TMergeMapPairVec;
    // maps a node's label to all the nodes that have that same label
    // (and are therefore mergeable)
    typedef map< string, TMergeMapPairVec > TMergeMap;

    TMergeMap merge_map;
    ITERATE( CTraversalNode::TNodeSet, node_iter, currentTier ) {
        string node_label;
        // A node's label is the same as another nodes iff
        // they can be merged
        CNodeLabeler::ERootEncountered root_encountered = 
            CNodeLabeler::eRootEncountered_No;
        CNodeLabeler( *node_iter, node_label, root_encountered, m_RootTraversalNodes );
        merge_map[node_label].push_back(
            TMergeMapPair( 
                root_encountered != CNodeLabeler::eRootEncountered_No, *node_iter ) );
    }
    
    // for each mergeable set of nodes, merge them and place their callers on 
    // the next tier
    NON_CONST_ITERATE( TMergeMap, merge_iter, merge_map ) {
        TMergeMapPairVec &merge_vec = merge_iter->second;
        if( merge_vec.size() > 1 ) {
            // merge all nodes into the node with the shortest func name,
            // or, if there's a root node, into the root node
            // ( Watch out for the case of multiple root nodes! )

            TMergeMapPairVec::iterator do_merge_iter =
                merge_vec.begin();
            // length of name, or negative if it's a root node, 
            // since we prefer root nodes
            int target_badness = kMax_Int;
            CRef<CTraversalNode> target = do_merge_iter->second;
            while( do_merge_iter != merge_vec.end() ) {
                CRef<CTraversalNode> current_node = do_merge_iter->second;
                const bool has_root = do_merge_iter->first;
                if( has_root ) {
                    // if there's already a root, then just remove 
                    // this one from the mergeable list
                    if( target_badness < 0 ) {
                        // This might be slow.  Maybe use lists instead?
                        // ( Performance empirically acceptable for now,
                        // though )
                        do_merge_iter = merge_vec.erase( do_merge_iter );
                        continue;
                    }
                    target = current_node;
                    target_badness = -1; // guarantee that we use the root
                } else if( (int)current_node->GetFuncName().length() < target_badness ) {
                    target = current_node;
                }
                ++do_merge_iter;
            }

            do_merge_iter = merge_vec.begin();
            for( ; do_merge_iter != merge_vec.end(); ++do_merge_iter ) {
                // see if the target itself is a root
                // (This is different from "has_root" which checks if
                // the node or any of its direct or indirect callees are a root )
                const bool target_is_itself_a_root = 
                    ( m_RootTraversalNodes.find(target) != m_RootTraversalNodes.end() );
                // "Merge()" will properly detect the case of trying to merge a node into itself
                target->Merge( do_merge_iter->second, 
                    ( target_is_itself_a_root ? 
                        CTraversalNode::eMergeNameAllowed_ForbidNameChange : 
                        CTraversalNode::eMergeNameAllowed_AllowNameChange ) );
            }
            x_AddCallersToTier( target, nextTier );
        }
    }
}

void CTraversalMerger::x_AddCallersToTier( 
    CRef<CTraversalNode> current_node,
    CTraversalNode::TNodeSet &tier )
{
    const CTraversalNode::TNodeCallSet &callers = current_node->GetCallers();
    ITERATE( CTraversalNode::TNodeCallSet, caller_iter, callers ) {
        tier.insert( (*caller_iter)->GetNode() );
    }
}

END_NCBI_SCOPE
