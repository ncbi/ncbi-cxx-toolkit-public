#ifndef ALGO___TREE__ALGO_HPP
#define ALGO___TREE__ALGO_HPP
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:
 *	 Tree algorithms
 *
 */

/// @file tree_algo.hpp
/// Tree algorithms

#include <corelib/ncbi_tree.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup Tree
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
//
//  General purpose tree algorithms
//





/// Trace from the specified node to to the tree root
///
/// Trace path is a container of node const node pointers 
/// (The only requirement is push_back method)
/// The input node becomes first element, then comes its parent.
/// If the node is a tree top its pointer is the only element of the trace
/// vector
///
/// @param tree_node  
///    Starting node (added to the trace first)
/// @param trace 
///    Trace container (vector<const TTreeNode*> or similar)
///
template<class TTreeNode, class TTraceContainer>
void TreeTraceToRoot(const TTreeNode& tree_node, TTraceContainer& trace)
{
    const TTreeNode* node_ptr = &tree_node;

    while (node_ptr) {
        trace.push_back(node_ptr);
        node_ptr = node_ptr->GetParent();
    }
}


/// Change tree root (tree rotation)
///
/// @param new_root_node
///    new node which becomes new tree root after the call
template<class TTreeNode>
void TreeReRoot(TTreeNode& new_root_node)
{
    vector<const TTreeNode*> trace;
    TreeTraceToRoot(new_root_node, trace);

    TTreeNode* local_root = &new_root_node;
    ITERATE(typename vector<const TTreeNode*>, it, trace) {
        TTreeNode* node = const_cast<TTreeNode*>(*it);
        TTreeNode* parent = node->GetParent();
        if (parent)
            parent->DetachNode(node);
        if (local_root != node)
            local_root->AddNode(node);
        local_root = node;
    }
}

/// Check if two nodes have the same common root.
///
/// Algorithm finds first common ancestor (farthest from the root)
/// @param tree_node_a
///     Node A
/// @param tree_node_b
///     Node B
/// @return 
///     Nearest common root or NULL if there is no common parent
///
template<class TTreeNode>
const TTreeNode* TreeFindCommonParent(const TTreeNode& tree_node_a,
                                      const TTreeNode& tree_node_b)
{
    typedef vector<const TTreeNode*>  TTraceVector;

    TTraceVector trace_a;
    TTraceVector trace_b;

    TreeTraceToRoot(tree_node_a, trace_a);
    TreeTraceToRoot(tree_node_b, trace_b);

    // trace comparison: go to the 

    const TTreeNode* node_candidate = 0;


    // We need this next variables because of a bug(?) in MSVC 6 
    //    vector::rbegin() returns non const reverse iterator 
    //    for a non const vector (sort of)

    const TTraceVector& ctr_a = trace_a;
    const TTraceVector& ctr_b = trace_b;

    typename TTraceVector::const_reverse_iterator it_a = ctr_a.rbegin();
    typename TTraceVector::const_reverse_iterator it_b = ctr_b.rbegin();

    typename TTraceVector::const_reverse_iterator it_a_end = ctr_a.rend();
    typename TTraceVector::const_reverse_iterator it_b_end = ctr_b.rend();
    
    for (;it_a != it_a_end || it_b != it_b_end; ++it_a, ++it_b) {
        const TTreeNode* node_a = *it_a;
        const TTreeNode* node_b = *it_b;

        if (node_a != node_b) {
            break;
        }
        node_candidate = node_a;        
    }

    return node_candidate;
}


/// Tree print functor used as a tree traversal payload
/// @internal
template<class TTreeNode, class TConverter>
class CTreePrintFunc
{
public:
    CTreePrintFunc(CNcbiOstream& os, TConverter& conv, bool print_ptr)
    : m_OStream(os),
      m_Conv(conv),
      m_Level(0),
      m_PrintPtr(print_ptr)
    {}

    ETreeTraverseCode 
    operator()(const TTreeNode& tr, int delta) 
    {
        m_Level += delta;
        if (delta >= 0) { 
            PrintLevelMargin();
            const string& node_str = m_Conv(tr.GetValue());

            if (m_PrintPtr) {
                m_OStream  
                   << "[" << "0x" << hex << &tr << "]=> 0x" << tr.GetParent() 
                   << " ( " << node_str << " ) " << endl;
            } else {
                m_OStream << node_str << endl;
            }
        }

        return eTreeTraverse;
    }

    void PrintLevelMargin()
    {
        for (int i = 0; i < m_Level; ++i) {
            m_OStream << "  ";
        }
    }
    
private:
    CNcbiOstream&  m_OStream;
    TConverter&    m_Conv;
    int            m_Level;
    bool           m_PrintPtr;
};


/// Tree printing (use for debugging purposes)
///
/// @param os
///     Stream to print the tree
/// @param tree_node
///     Tree node to print (top or sub-tree)
/// @param conv
///     Converter of node value to string (string Conv(node_value) )
/// @param print_ptr
///     When TRUE function prints all internal pointers (debugging)
///
template<class TTreeNode, class TConverter>
void TreePrint(CNcbiOstream&     os, 
               const TTreeNode&  tree_node, 
               TConverter        conv,
               bool              print_ptr = false)
{
    // fake const_cast, nothing to worry 
    //   (should go away when we have const traverse)
    TTreeNode* non_c_tr = const_cast<TTreeNode*>(&tree_node);
    CTreePrintFunc<TTreeNode, TConverter> func(os, conv, print_ptr);
    TreeDepthFirstTraverse(*non_c_tr, func);
}



/////////////////////////////////////////////////////////////////////////////
//
//  Algorithms for Id trees
//


/// Algorithm to to search for a node with specified id
///
/// Tree is traversed back. When searching the upper(parent)
/// level the algorithm never goes down the hierarchy but tries only 
/// immediate children.
///
template<class TPairTree, class TValue>
const TPairTree* PairTreeBackTraceNode(const TPairTree& tr, 
                                       const TValue&    search_id)
{
    const TPairTree* ptr = &tr;

    do {
        const typename TPairTree::TValueType& node_value = ptr->GetValue();

        if (search_id == node_value) {
            return (TPairTree*) ptr;
        }

        typename TPairTree::TNodeList_CI it = ptr->SubNodeBegin();
        typename TPairTree::TNodeList_CI it_end = ptr->SubNodeEnd();

        for (;it != it_end; ++it) {
            const typename TPairTree::TParent* node = *it;
            const typename TPairTree::TTreePair& node_pair = node->GetValue();

            if (search_id == node_pair.GetId()) {
                return (TPairTree*) node;
            }

        } // for

        ptr = (TPairTree*)ptr->GetParent();

    } while (ptr);

    return 0;
}


/// Algorithm to trace the pair tree and find specified leaf along the node path
/// 
/// Algorithm always chooses the deepest leaf 
template<class TPairTree, class TPathList>
const TPairTree* PairTreeTraceNode(const TPairTree& tr, const TPathList& node_path)
{
    _ASSERT(!node_path.empty());

    const TPairTree* ptr = &tr;
    const TPairTree* pfnd = 0;

    typename TPathList::const_iterator last_it = node_path.end();
    --last_it;
    const typename TPathList::value_type& last_search_id = *last_it;

    ITERATE(typename TPathList, sit, node_path) {
        const typename TPairTree::TIdType& search_id = *sit;
        bool sub_level_found = false;
        
        typename TPairTree::TNodeList_CI it = ptr->SubNodeBegin();
        typename TPairTree::TNodeList_CI it_end = ptr->SubNodeEnd();

        for (;it != it_end; ++it) {
            const typename TPairTree::TParent* node = *it;
            const typename TPairTree::TTreePair& node_pair = node->GetValue();

            
            if (node_pair.GetId() == search_id) {  
                ptr = (TPairTree*) node;
                sub_level_found = true;
            }

            if (node_pair.id == last_search_id) {
                pfnd = (TPairTree*) node;
                sub_level_found = true;
            }

            if (sub_level_found)
                break;

        } // for it

        if (!sub_level_found) {
            break;
        }
        
    } // ITERATE

    return pfnd;
}


/* @} */

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/04/19 16:02:06  kuznets
 * Initial revision. Migrated from <corelib/ncbi_tree.hpp>
 *
 *
 * ==========================================================================
 */

#endif
