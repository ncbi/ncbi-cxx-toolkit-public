#ifndef CORELIB___NCBI_TREE__HPP
#define CORELIB___NCBI_TREE__HPP
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
 *	 Tree container.
 *
 */

#include <corelib/ncbistd.hpp>
#include <list>
#include <stack>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
///    Bi-directionaly linked N way tree.
///

template <class TValue> class CTreeNode
{
public:
    typedef TValue                     TValueType;
    typedef CTreeNode<TValue>          TTreeType;
    typedef list<TTreeType*>           TNodeList;
    typedef list<const TTreeType*>     TConstNodeList;
    typedef typename TNodeList::iterator        TNodeList_I;
    typedef typename TNodeList::const_iterator  TNodeList_CI;

    /// Tree node construction
    ///
    /// @param
    ///   value - node value
    CTreeNode(const TValue& value = TValue());
    ~CTreeNode();

    CTreeNode(const TTreeType& tree);
    CTreeNode& operator =(const TTreeType& tree);

    /// Get node's parent
    ///
    /// @return parent to the current node, NULL if it is a top
    /// of the tree
    const TTreeType* GetParent() const { return m_Parent; }

    /// Get node's parent
    ///
    /// @return parent to the current node, NULL if it is a top
    /// of the tree
    TTreeType* GetParent() { return m_Parent; }

    /// Return first const iterator on subnode list
    TNodeList_CI SubNodeBegin() const { return m_Nodes.begin(); }

    /// Return first iterator on subnode list
    TNodeList_I SubNodeBegin() { return m_Nodes.begin(); }

    /// Return last const iterator on subnode list
    TNodeList_CI SubNodeEnd() const { return m_Nodes.end(); }

    /// Return last iterator on subnode list
    TNodeList_I SubNodeEnd() { return m_Nodes.end(); }

    /// Return node's value
    const TValue& GetValue() const { return m_Value; }

    /// Return node's value
    TValue& GetValue() { return m_Value; }

    /// Set value for the node
    void SetValue(const TValue& value) { m_Value = value; }

    /// Remove subnode of the current node. Must be direct subnode.
    ///
    /// If subnode is not connected directly with the current node
    /// the call has no effect.
    ///
    /// @param 
    ///    subnode  direct subnode pointer
    void RemoveNode(TTreeType* subnode);

    /// Remove subnode of the current node. Must be direct subnode.
    ///
    /// If subnode is not connected directly with the current node
    /// the call has no effect.
    ///
    /// @param 
    ///    it  subnode iterator
    void RemoveNode(TNodeList_I it);

    enum EDeletePolicy
    {
        eDelete,
        eNoDelete
    };

    /// Remove all immediate subnodes
    ///
    /// @param del
    ///    Subnode delete policy
    void RemoveAllSubNodes(EDeletePolicy del = eDelete);

    /// Remove the subtree from the tree without freeing it
    ///
    /// If subnode is not connected directly with the current node
    /// the call has no effect. The caller is responsible for deletion
    /// of the returned subtree.
    ///
    /// @param 
    ///    subnode  direct subnode pointer
    ///
    /// @return subtree pointer or NULL if requested subnode does not exist
    TTreeType* DetachNode(TTreeType* subnode);

    /// Remove the subtree from the tree without freeing it
    ///
    /// If subnode is not connected directly with the current node
    /// the call has no effect. The caller is responsible for deletion
    /// of the returned subtree.
    ///
    /// @param 
    ///    subnode  direct subnode pointer
    ///
    /// @return subtree pointer or NULL if requested subnode does not exist
    TTreeType* DetachNode(TNodeList_I it);

    /// Add new subnode
    ///
    /// @param 
    ///    subnode subnode pointer
    void AddNode(TTreeType* subnode);


    /// Add new subnode whose value is (a copy of) val
    ///
    /// @param 
    ///    val value reference
    ///
    /// @return pointer to new subtree
    CTreeNode<TValue>* AddNode(const TValue& val);


    /// Insert new subnode before the specified location in the subnode list
    ///
    /// @param
    ///    it subnote iterator idicates the location of the new subtree
    /// @param 
    ///    subnode subtree pointer
    void InsertNode(TNodeList_I it, TTreeType* subnode);

    /// Report whether this is a leaf node
    ///
    /// @return true if this is a leaf node (has no children),
    /// false otherwise
    bool IsLeaf() const { return m_Nodes.empty(); };

protected:
    void CopyFrom(const TTreeType& tree);
    void SetParent(TTreeType* parent_node) { m_Parent = parent_node; }

    const TNodeList& GetSubNodes() const { return m_Nodes; }

protected:
    TTreeType*         m_Parent; ///< Pointer on the parent node
    TNodeList          m_Nodes;  ///< List of dependent nodes
    TValue             m_Value;  ///< Node value
};


template <class TId, class TValue>
struct CTreePair
{
    TId       id;
    TValue    value;

    CTreePair() {}
    CTreePair(const TId& tid, const TValue& tvalue)
    : id(tid),
      value(tvalue)
    {}
};

/////////////////////////////////////////////////////////////////////////////
///
///    Bi-directionaly linked N way tree.
///    Parameterized by id - value pair

template <class TId, class TValue> class CTreePairNode
    : public CTreeNode< CTreePair<TId, TValue> >
{
public:
    typedef TId                                  TIdType;
    typedef TValue                               TValueType;
    typedef CTreeNode<CTreePair<TId, TValue> >   TParent;
    typedef CTreePair<TId, TValue>               TTreePair;
    typedef CTreePairNode<TId, TValue>           TPairTreeType;
    typedef list<TPairTreeType*>                 TPairTreeNodeList;
    typedef list<const TPairTreeType*>           TConstPairTreeNodeList;

public:

    CTreePairNode(const TId& id = TId(), const TValue& value = TValue());
    CTreePairNode(const CTreePairNode<TId, TValue>& tr);
    CTreePairNode<TId, TValue>& operator=(const CTreePairNode<TId, TValue>& tr);


    /// Add new subnode whose value is (a copy of) val
    ///
    /// @param 
    ///    val value reference
    ///
    /// @return pointer to new subtree
    CTreePairNode<TId, TValue>* AddNode(const TId& id, const TValue& value);

    /// Return node's value
    const TValueType& GetValue() const { return m_Value.value; }

    /// Return node's id
    const TIdType& GetId() const { return m_Value.id; }

    /// Return node's value
    TValueType& GetValue() { return m_Value.value; }

    /// Return node's id
    TIdType& GetId() { return m_Value.id; }

    /// Set value for the node
    void SetValue(const TValue& value) { m_Value.value = value; }

    /// Set id for the node
    void SetId(const TId& id) { m_Value.id = id; }

    /// Find tree nodes corresponding to the path from the top
    ///
    /// @param node_path
    ///    hierachy of node ids to search for
    /// @param res
    ///    list of discovered found nodes
    void FindNodes(const list<TId>& node_path, TPairTreeNodeList* res);

    /// Find tree nodes corresponding to the path from the top
    ///
    /// @param node_path
    ///    hierachy of node ids to search for
    /// @param res
    ///    list of discovered found nodes (const pointers)
    void FindNodes(const list<TId>& node_path, TConstPairTreeNodeList* res) const;
};

/////////////////////////////////////////////////////////////////////////////
//
//  Tree algorithms
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

            if (search_id == node_pair.id) {
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

            if (node_pair.id == search_id) {  
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

/// Tree traverse code returned by the traverse predicate function
enum ETreeTraverseCode
{
    eTreeTraverse,           ///< Keep traversal
    eTreeTraverseStop,       ///< Stop traversal (return form algorithm)
    eTreeTraverseStepOver    ///< Do not traverse current node (pick the next one)
};


/// Depth-first tree traversal algorithm.
///
/// Takes tree and visitor function and calls function for every 
/// node in the tree.
///
/// Functor should have the next prototype:
/// ETreeTraverseCode Func(TreeNode& node, int delta_level)
///  where node is a reference to the visited node and delta_level 
///  reflects the current traverse direction(depth wise) in the tree, 
///   0  - algorithm stays is on the same level
///   1  - we are going one level deep into the tree (from the root)
///  -1  - we are traveling back by one level (getting close to the root)
///
/// Algorithm calls the visitor function on the way back so it revisits
/// some tree nodes. It is possible to implement both variants of tree 
/// traversal (pre-order and post-order)
/// When visitor decides to stop the traverse it returns TRUE.
///
template<class TTreeNode, class Fun>
Fun TreeDepthFirstTraverse(TTreeNode& tree_node, Fun func)
{
    int delta_level = 0;
    ETreeTraverseCode stop_scan;

    stop_scan = func(tree_node, delta_level);
    switch (stop_scan) {
        case eTreeTraverseStop:
        case eTreeTraverseStepOver:
            return func;
        case eTreeTraverse:
            break;
    }

    if (stop_scan)
        return func;

    delta_level = 1;
    TTreeNode* tr = &tree_node;

    typedef typename TTreeNode::TNodeList_I TTreeNodeIterator;

    TTreeNodeIterator it = tr->SubNodeBegin();
    TTreeNodeIterator it_end = tr->SubNodeEnd();

    stack<TTreeNodeIterator> tree_stack;

    while (true) {
        tr = *it;
        stop_scan = eTreeTraverse;
        if (tr) {
            stop_scan = func(*tr, delta_level);
            switch (stop_scan) {
                case eTreeTraverseStop:
                    return func;
                case eTreeTraverse:
                    break;
            }
        }
        if ( (stop_scan != eTreeTraverseStepOver) &&
             (delta_level >= 0) && 
             (!tr->IsLeaf())) {  // sub-node, going down
            tree_stack.push(it);
            it = tr->SubNodeBegin();
            it_end = tr->SubNodeEnd();
            delta_level = 1;
            continue;
        }
        ++it;
        if (it == it_end) { // end of level, going up
            if (tree_stack.empty()) {
                break;
            }
            it = tree_stack.top();
            tree_stack.pop();
            tr = *it;
            it_end = tr->GetParent()->SubNodeEnd();
            delta_level = -1;
            continue;
        }
        // same level 
        delta_level = 0;

    } // while

    func(tree_node, -1);
    return func;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTreeNode<TValue>
//

template<class TValue>
CTreeNode<TValue>::CTreeNode(const TValue& value)
: m_Parent(0),
  m_Value(value)
{}

template<class TValue>
CTreeNode<TValue>::~CTreeNode()
{
    ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNode* node = *it;
        delete node;
    }
}

template<class TValue>
CTreeNode<TValue>::CTreeNode(const TTreeType& tree)
: m_Parent(0),
  m_Value(tree.m_Value)
{
    CopyFrom(tree);
}

template<class TValue>
CTreeNode<TValue>& CTreeNode<TValue>::operator=(const TTreeType& tree)
{
    NON_CONST_ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNode* node = *it;
        delete node;
    }
    m_Nodes.clear();
    CopyFrom(tree);
}

template<class TValue>
void CTreeNode<TValue>::CopyFrom(const TTreeType& tree)
{
    ITERATE(typename TNodeList, it, tree.m_Nodes) {
        const CTreeNode* src_node = *it;
        CTreeNode* new_node = new CTreeNode(*src_node);
        AddNode(new_node);
    }
}

template<class TValue>
void CTreeNode<TValue>::RemoveNode(TTreeType* subnode)
{
    NON_CONST_ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNode* node = *it;
        if (node == subnode) {
            m_Nodes.erase(it);
            delete node;
            break;
        }
    }    
}

template<class TValue>
void CTreeNode<TValue>::RemoveNode(TNodeList_I it)
{
    CTreeNode* node = *it;
    m_Nodes.erase(it);
    delete node;
}


template<class TValue>
typename CTreeNode<TValue>::TTreeType* 
CTreeNode<TValue>::DetachNode(typename CTreeNode<TValue>::TTreeType* subnode)
{
    NON_CONST_ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNode* node = *it;
        if (node == subnode) {
            m_Nodes.erase(it);
            node->SetParent(0);
            return node;
        }
    }        
    return 0;
}


template<class TValue>
typename CTreeNode<TValue>::TTreeType* 
CTreeNode<TValue>::DetachNode(typename CTreeNode<TValue>::TNodeList_I it)
{
    CTreeNode* node = *it;
    m_Nodes.erase(it);
    node->SetParent(0);

    return node;
}


template<class TValue>
void CTreeNode<TValue>::AddNode(typename CTreeNode<TValue>::TTreeType* subnode)
{
    m_Nodes.push_back(subnode);
    subnode->SetParent(this);
}

template<class TValue>
CTreeNode<TValue>* CTreeNode<TValue>::AddNode(const TValue& val)
{
    TTreeType* subnode = new TTreeType(val);
    AddNode(subnode);
    return subnode;
}

template<class TValue>
void CTreeNode<TValue>::InsertNode(TNodeList_I it,
                                   TTreeType* subnode)
{
    m_Nodes.insert(it, subnode);
    subnode->SetParent(this);
}

template<class TValue>
void CTreeNode<TValue>::RemoveAllSubNodes(EDeletePolicy del)
{
    if (del == eDelete) {
        NON_CONST_ITERATE(typename TNodeList, it, m_Nodes) {
            CTreeNode* node = *it;
            delete node;
        }
    }
    m_Nodes.clear();
}

/////////////////////////////////////////////////////////////////////////////
//
//  CTreePairNode<TId, TValue>
//


template<class TId, class TValue>
CTreePairNode<TId, TValue>::CTreePairNode(const TId& id, const TValue& value)
 : TParent(TTreePair(id, value))
{}


template<class TId, class TValue>
CTreePairNode<TId, TValue>::CTreePairNode(const CTreePairNode<TId, TValue>& tr)
: TParent(tr)
{}


template<class TId, class TValue>
CTreePairNode<TId, TValue>& 
CTreePairNode<TId, TValue>::operator=(const CTreePairNode<TId, TValue>& tr)
{
    TParent::operator=(tr);
}

template<class TId, class TValue>
CTreePairNode<TId, TValue>*
CTreePairNode<TId, TValue>::AddNode(const TId& id, const TValue& value)
{
    return (CTreePairNode<TId, TValue>*)TParent::AddNode(TTreePair(id, value));
}

template<class TId, class TValue>
void CTreePairNode<TId, TValue>::FindNodes(const list<TId>& node_path, 
                                           TPairTreeNodeList*       res)
{
    typename TParent::TTreeType* tr = this;

    ITERATE(typename list<TId>, sit, node_path) {
        const TId& search_id = *sit;
        bool sub_level_found = false;

        typename TParent::TNodeList_I it = tr->SubNodeBegin();
        typename TParent::TNodeList_I it_end = tr->SubNodeEnd();

        for (; it != it_end; ++it) {
            TParent* node = *it;
            const TTreePair& node_pair = node->GetValue();

            if (node_pair.id == search_id) {
                tr = node;
                sub_level_found = true;
                break;
            }
        } // for it

        if (!sub_level_found) {
            return;
        }
        sub_level_found = false;

    } // ITERATE

    res.push_back(tr);
}


template<class TId, class TValue>
void CTreePairNode<TId, TValue>::FindNodes(const list<TId>&         node_path, 
                                           TConstPairTreeNodeList*  res) const
{
    const typename TParent::TTreeType* tr = this;

    ITERATE(typename list<TId>, sit, node_path) {
        const TId& search_id = *sit;
        bool sub_level_found = false;

        typename TParent::TNodeList_CI it = tr->SubNodeBegin();
        typename TParent::TNodeList_CI it_end = tr->SubNodeEnd();

        for (; it != it_end; ++it) {
            const TParent* node = *it;
            const TTreePair& node_pair = node->GetValue();

            if (node_pair.id == search_id) {
                tr = node;
                sub_level_found = true;
                break;
            }
        } // for it

        if (!sub_level_found) {
            return;
        }
        sub_level_found = false;

    } // ITERATE

    res->push_back((TPairTreeType*)tr);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2004/01/14 17:38:05  kuznets
 * TreeDepthFirstTraverse improved to support more traversal options
 * (ETreeTraverseCode)
 *
 * Revision 1.18  2004/01/14 17:02:32  kuznets
 * + PairTreeBackTraceNode
 *
 * Revision 1.17  2004/01/14 16:24:17  kuznets
 * + CTreeNode::RemoveAllSubNodes
 *
 * Revision 1.16  2004/01/14 15:25:38  kuznets
 * Fixed bug in PairTreeTraceNode algorithm
 *
 * Revision 1.15  2004/01/14 14:18:21  kuznets
 * +TreeDepthFirstTraverse algorithm
 *
 * Revision 1.14  2004/01/12 22:04:03  ucko
 * Fix typo caught by MIPSpro.
 *
 * Revision 1.13  2004/01/12 21:03:42  ucko
 * Fix use of typename in PairTreeTraceNode<>, silently added in last revision.
 *
 * Revision 1.12  2004/01/12 20:09:22  kuznets
 * Renamed CTreeNWay to CTreeNode
 *
 * Revision 1.11  2004/01/12 18:01:15  jcherry
 * Added IsLeaf method
 *
 * Revision 1.10  2004/01/12 16:49:48  kuznets
 * CTreePairNode added id, value accessor functions
 *
 * Revision 1.9  2004/01/12 15:26:22  kuznets
 * Fixed various compilation warnings (GCC & WorkShop)
 *
 * Revision 1.8  2004/01/12 15:01:58  kuznets
 * +CTreePairNode::FindNodes
 *
 * Revision 1.7  2004/01/09 19:01:39  kuznets
 * Fixed compilation for GCC
 *
 * Revision 1.6  2004/01/09 17:15:11  kuznets
 * Cosmetic cleanup
 *
 * Revision 1.5  2004/01/09 13:27:39  kuznets
 * Cosmetic fixes. nodelist_iterator renamed to match the NCBI coding policy.
 *
 * Revision 1.4  2004/01/07 21:38:29  jcherry
 * Added method for adding child node given a value
 *
 * Revision 1.3  2004/01/07 21:10:58  jcherry
 * Compile fixes
 *
 * Revision 1.2  2004/01/07 17:21:53  kuznets
 * + template implementation
 *
 * Revision 1.1  2004/01/07 13:17:10  kuznets
 * Initial revision
 *
 *
 * ==========================================================================
 */

#endif
