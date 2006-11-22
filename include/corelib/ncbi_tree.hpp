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
 *     Tree container.
 *
 */

#include <corelib/ncbistd.hpp>
#include <list>
#include <stack>
#include <queue>
#include <deque>

BEGIN_NCBI_SCOPE

/** @addtogroup Tree
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///    Bi-directionaly linked N way tree.
///

/// Default key getter for CTreeNode
template <class TValue>
class CDefaultNodeKeyGetter
{
public:
    typedef TValue TNodeType;   ///< same as CTreeNode template argument
    typedef TValue TValueType;  ///< node's value
    typedef TValue TKeyType;    ///< node's key

    /// Get value of a node
    static const TValueType& GetValue(const TNodeType& node) { return node; }
    /// Get non-const value of a node
    static TValueType& GetValueNC(TNodeType& node) { return node; }
    /// Get key of a node
    static const TKeyType& GetKey(const TNodeType& node) { return node; }
    /// Get non-const key
    static TKeyType& GetKeyNC(TNodeType& node) { return node; }
    /// Check if the two keys are equal
    static bool KeyCompare(const TKeyType& key1, const TKeyType& key2)
        { return key1 == key2; }
};


template <class TValue, class TKeyGetter = CDefaultNodeKeyGetter<TValue> >
class CTreeNode
{
public:
    typedef TValue                              TValueType;
    typedef typename TKeyGetter::TKeyType       TKeyType;
    typedef CTreeNode<TValue, TKeyGetter>       TTreeType;
    typedef list<TTreeType*>                    TNodeList;
    typedef list<const TTreeType*>              TConstNodeList;
    typedef typename TNodeList::iterator        TNodeList_I;
    typedef typename TNodeList::const_iterator  TNodeList_CI;
    typedef typename TNodeList::reverse_iterator        TNodeList_RI;
    typedef typename TNodeList::const_reverse_iterator  TNodeList_CRI;
    typedef list<TKeyType>                      TKeyList;

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
    const TTreeType* GetParent(void) const { return m_Parent; }

    /// Get node's parent
    ///
    /// @return parent to the current node, NULL if it is a top
    /// of the tree
    TTreeType* GetParent(void) { return m_Parent; }

    /// Get the topmost node 
    ///
    /// @return global parent of the current node, this if it is a top
    /// of the tree
    const TTreeType* GetRoot(void) const;

    /// Get the topmost node 
    ///
    /// @return global parent of the current node, this if it is a top
    /// of the tree
    TTreeType* GetRoot(void);


    /// Return first const iterator on subnode list
    TNodeList_CI SubNodeBegin(void) const { return m_Nodes.begin(); }

    /// Return first iterator on subnode list
    TNodeList_I SubNodeBegin(void) { return m_Nodes.begin(); }

    /// Return last const iterator on subnode list
    TNodeList_CI SubNodeEnd(void) const { return m_Nodes.end(); }

    /// Return last iterator on subnode list
    TNodeList_I SubNodeEnd(void) { return m_Nodes.end(); }

    /// Return first const reverse iterator on subnode list
    TNodeList_CRI SubNodeRBegin(void) const { return m_Nodes.rbegin(); }

    /// Return first reverse iterator on subnode list
    TNodeList_RI SubNodeRBegin(void) { return m_Nodes.rbegin(); }

    /// Return last const reverse iterator on subnode list
    TNodeList_CRI SubNodeREnd(void) const { return m_Nodes.rend(); }

    /// Return last reverse iterator on subnode list
    TNodeList_RI SubNodeREnd(void) { return m_Nodes.rend(); }

    /// Return node's value
    const TValue& GetValue(void) const { return m_Value; }

    /// Return node's value
    TValue& GetValue(void) { return m_Value; }

    /// Set value for the node
    void SetValue(const TValue& value) { m_Value = value; }

    const TValue& operator*(void) const { return m_Value; }
    TValue& operator*(void) { return m_Value; }
    const TValue* operator->(void) const { return &m_Value; }
    TValue* operator->(void) { return &m_Value; }

    const TKeyType& GetKey(void) const { return TKeyGetter::GetKey(m_Value); }
    TKeyType& GetKey(void) { return TKeyGetter::GetKeyNC(m_Value); }

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
    ///    Subnode delete policy. When delete policy is "eNoDelete"
    ///    nodes are just excluded from the tree. It is responsibility
    ///    of the caller to track them and guarantee proper destruction.
    ///
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
    TTreeType* AddNode(const TValue& val = TValue());

    /// Remove all subnodes from the source node and attach them to the
    /// current tree (node). Source node cannot be an ancestor of the 
    /// current node
    void MoveSubnodes(TTreeType* src_tree_node);

    /// Insert new subnode before the specified location in the subnode list
    ///
    /// @param
    ///    it subnote iterator idicates the location of the new subtree
    /// @param 
    ///    subnode subtree pointer
    void InsertNode(TNodeList_I it, TTreeType* subnode);

    /// Report whether this is a leaf node
    ///
    /// @return TRUE if this is a leaf node (has no children),
    /// false otherwise
    bool IsLeaf() const { return m_Nodes.empty(); };

    /// Check if node is a direct or indirect parent of this node
    ///
    /// @param  tree_node
    ///    Node candidate
    /// @return TRUE if tree_node is a direct or indirect parent
    bool IsParent(const TTreeType& tree_node) const;

    /// Find tree nodes corresponding to the path from the top
    ///
    /// @param node_path
    ///    hierachy of node keys to search for
    /// @param res
    ///    list of discovered found nodes
    void FindNodes(const TKeyList& node_path, TNodeList* res);

    /// Find tree nodes corresponding to the path from the top
    ///
    /// @param node_path
    ///    hierachy of node keys to search for
    /// @param res
    ///    list of discovered found nodes (const pointers)
    void FindNodes(const TKeyList& node_path, 
                   TConstNodeList* res) const;

    /// Non recursive linear scan of all subnodes, with key comparison
    ///
    /// @return SubNode pointer or NULL
    const TTreeType* FindSubNode(const TKeyType& key) const;

    /// Parameters for node search by key
    ///
    enum ENodeSearchType
    {
        eImmediateSubNodes = (1 << 0),  ///< Search direct subnodes
        eTopLevelNodes     = (1 << 1),  ///< Search subnodes of the root
        eAllUpperSubNodes  = (1 << 2),  ///< Search all subnodes on the way up

        eImmediateAndTop = (eImmediateSubNodes | eTopLevelNodes)
    };

    typedef int TNodeSearchMode; ///< Recombination of ENodeSearchType

    /// Search for node
    ///
    /// @param sflag
    ///     ORed ENodeSearchType
    /// @return node pointer or NULL
    const TTreeType* FindNode(const TKeyType& key, 
                              TNodeSearchMode sflag = eImmediateAndTop) const;

protected:
    void CopyFrom(const TTreeType& tree);
    void SetParent(TTreeType* parent_node) { m_Parent = parent_node; }

    const TNodeList& GetSubNodes() const { return m_Nodes; }

protected:
    TTreeType*         m_Parent; ///< Pointer on the parent node
    TNodeList          m_Nodes;  ///< List of dependent nodes
    TValue             m_Value;  ///< Node value
};


/// Default key getter for pair-node (id + value)
template <class TNode>
class CPairNodeKeyGetter
{
public:
    typedef TNode                      TNodeType;
    typedef typename TNode::TValueType TValueType;
    typedef typename TNode::TIdType    TKeyType;

    static const TValueType& GetValue(const TNodeType& node)
        { return node.value; }
    static TValueType& GetValueNC(TNodeType& node)
        { return node.value; }
    static const TKeyType& GetKey(const TNodeType& node)
        { return node.id; }
    static TKeyType& GetKeyNC(TNodeType& node)
        { return node.id; }
    static bool KeyCompare(const TKeyType& key1, const TKeyType& key2)
        { return key1 == key2; }
};


/// Node data template for id-value trees
template <class TId, class TValue>
struct CTreePair
{
    // typedefs for CPairNodeKeyGetter
    typedef TId     TIdType;
    typedef TValue  TValueType;

    /// Node data type
    typedef CTreePair<TId, TValue>                TTreePair;
    /// Key getter for CTreeNode
    typedef CPairNodeKeyGetter<TTreePair>         TPairKeyGetter;
    /// Tree node type
    typedef CTreeNode<TTreePair, TPairKeyGetter>  TPairTreeNode;

    CTreePair() {}
    CTreePair(const TId& aid, const TValue& avalue = TValue())
    : id(aid),
      value(avalue)
    {}

    TId    id;
    TValue value;
};


/////////////////////////////////////////////////////////////////////////////
//
//  Tree algorithms
//


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
/// The specificts of the algorithm is that it calls visitor both on the 
/// way from the root to leafs and on the way back
/// Using this template we can implement both variants of tree 
/// traversal (pre-order and post-order)
/// Visitor controls the traversal by returning ETreeTraverseCode
///
/// @sa ETreeTraverseCode
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

    if (it == it_end)
        return func;

    stack<TTreeNodeIterator> tree_stack;

    while (true) {
        tr = (TTreeNode*)*it;
        stop_scan = eTreeTraverse;
        if (tr) {
            stop_scan = func(*tr, delta_level);
            switch (stop_scan) {
                case eTreeTraverseStop:
                    return func;
                case eTreeTraverse:
                case eTreeTraverseStepOver:
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
            tr = (TTreeNode*)*it;
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

/// Breadth-first tree traversal algorithm.
///
/// Takes tree and visitor function and calls function for every 
/// node in the tree in breadth-first order. Functor is evaluated
/// at each node only once.
///
/// Functor should have the next prototype:
/// ETreeTraverseCode Func(TreeNode& node)
///    where node is a reference to the visited node 
/// @sa ETreeTraverseCode
///
template<class TTreeNode, class Fun>
Fun TreeBreadthFirstTraverse(TTreeNode& tree_node, Fun func)
{
    ETreeTraverseCode stop_scan;

    stop_scan = func(tree_node);
    switch(stop_scan) {
        case eTreeTraverseStop:
        case eTreeTraverseStepOver:
            return func;
        case eTreeTraverse:
            break;
    } 

    if ( stop_scan )
        return func;

    TTreeNode* tr = &tree_node;
  
    typedef typename TTreeNode::TNodeList_I TTreeNodeIterator;

    TTreeNodeIterator it = tr->SubNodeBegin();
    TTreeNodeIterator it_end = tr->SubNodeEnd();

    if (it == it_end)
        return func;

    queue<TTreeNodeIterator> tree_queue;

    while (it != it_end) 
        tree_queue.push(it++);
 
    while (!tree_queue.empty()) {

        it = tree_queue.front(); // get oldest node on queue
        tr = *it;
        tree_queue.pop(); // take oldest node off
        stop_scan = eTreeTraverse;
        if (tr) {
            stop_scan = func(*tr);
            switch(stop_scan) {
                case eTreeTraverseStop:
                    return func;
                case eTreeTraverse:
                case eTreeTraverseStepOver:
                    break;
            }
        }
        // Add children (if any) of node to queue
        if (stop_scan != eTreeTraverseStepOver  &&  !tr->IsLeaf()) { 
            it = tr->SubNodeBegin();
            it_end = tr->SubNodeEnd();
            while (it != it_end)
                tree_queue.push(it++);
        }
    }
    return func;
}




/////////////////////////////////////////////////////////////////////////////
//
//  CTreeNode<TValue>
//

template<class TValue, class TKeyGetter>
CTreeNode<TValue, TKeyGetter>::CTreeNode(const TValue& value)
    : m_Parent(0), m_Value(value)
{
}

template<class TValue, class TKeyGetter>
CTreeNode<TValue, TKeyGetter>::~CTreeNode(void)
{
    _ASSERT(m_Parent == 0);
    NON_CONST_ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNode* node = *it;
        node->m_Parent = 0;
        delete node;
    }
}

template<class TValue, class TKeyGetter>
CTreeNode<TValue, TKeyGetter>::CTreeNode(const TTreeType& tree)
: m_Parent(0),
  m_Value(tree.m_Value)
{
    CopyFrom(tree);
}

template<class TValue, class TKeyGetter>
CTreeNode<TValue, TKeyGetter>&
CTreeNode<TValue, TKeyGetter>::operator=(const TTreeType& tree)
{
    NON_CONST_ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNode* node = *it;
        node->m_Parent = 0;
        delete node;
    }
    m_Nodes.clear();
    CopyFrom(tree);
    return *this;
}

template<class TValue, class TKeyGetter>
void CTreeNode<TValue, TKeyGetter>::CopyFrom(const TTreeType& tree)
{
    ITERATE(typename TNodeList, it, tree.m_Nodes) {
        const CTreeNode* src_node = *it;
        CTreeNode* new_node = new CTreeNode(*src_node);
        AddNode(new_node);
    }
}

template<class TValue, class TKeyGetter>
void CTreeNode<TValue, TKeyGetter>::RemoveNode(TTreeType* subnode)
{
    NON_CONST_ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNode* node = *it;
        if (node == subnode) {
            m_Nodes.erase(it);
            node->m_Parent = 0;
            delete node;
            break;
        }
    }    
}

template<class TValue, class TKeyGetter>
void CTreeNode<TValue, TKeyGetter>::RemoveNode(TNodeList_I it)
{
    CTreeNode* node = *it;
    node->m_Parent = 0;
    m_Nodes.erase(it);
    delete node;
}

template<class TValue, class TKeyGetter>
typename CTreeNode<TValue, TKeyGetter>::TTreeType*
CTreeNode<TValue, TKeyGetter>::DetachNode(TTreeType* subnode)
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

template<class TValue, class TKeyGetter>
typename CTreeNode<TValue, TKeyGetter>::TTreeType*
CTreeNode<TValue, TKeyGetter>::DetachNode(TNodeList_I it)
{
    CTreeNode* node = *it;
    m_Nodes.erase(it);
    node->SetParent(0);

    return node;
}

template<class TValue, class TKeyGetter>
void CTreeNode<TValue, TKeyGetter>::AddNode(TTreeType* subnode)
{
    _ASSERT(subnode != this);
    m_Nodes.push_back(subnode);
    subnode->SetParent(this);
}

template<class TValue, class TKeyGetter>
typename CTreeNode<TValue, TKeyGetter>::TTreeType*
CTreeNode<TValue, TKeyGetter>::AddNode(const TValue& val)
{
    TTreeType* subnode = new TTreeType(val);
    AddNode(subnode);
    return subnode;
}


template<class TValue, class TKeyGetter>
void CTreeNode<TValue, TKeyGetter>::MoveSubnodes(TTreeType* src_tree_node)
{
    _ASSERT(!IsParent(*src_tree_node));
    TNodeList& src_nodes = src_tree_node->m_Nodes;
    ITERATE(typename TNodeList, it, src_nodes) {
        AddNode(*it);
    }
    src_nodes.clear();
}


template<class TValue, class TKeyGetter>
void CTreeNode<TValue, TKeyGetter>::InsertNode(TNodeList_I it,
                                               TTreeType* subnode)
{
    m_Nodes.insert(it, subnode);
    subnode->SetParent(this);
}

template<class TValue, class TKeyGetter>
void CTreeNode<TValue, TKeyGetter>::RemoveAllSubNodes(EDeletePolicy del)
{
    if (del == eDelete) {
        NON_CONST_ITERATE(typename TNodeList, it, m_Nodes) {
            CTreeNode* node = *it;
            node->m_Parent = 0;
            delete node;
        }
    }
    m_Nodes.clear();
}

template<class TValue, class TKeyGetter>
const typename CTreeNode<TValue, TKeyGetter>::TTreeType*
CTreeNode<TValue, TKeyGetter>::GetRoot() const
{
    const TTreeType* node_ptr = this;
    while (true) {
        const TTreeType* parent = node_ptr->GetParent();
        if (parent)
            node_ptr = parent;
        else
            break;
    }
    return node_ptr;
}

template<class TValue, class TKeyGetter>
typename CTreeNode<TValue, TKeyGetter>::TTreeType*
CTreeNode<TValue, TKeyGetter>::GetRoot()
{
    TTreeType* node_ptr = this;
    while (true) {
        TTreeType* parent = node_ptr->GetParent();
        if (parent) 
            node_ptr = parent;
        else 
            break;
    }
    return node_ptr;
}

template<class TValue, class TKeyGetter>
bool CTreeNode<TValue, TKeyGetter>::IsParent(const TTreeType& tree_node) const
{
    _ASSERT(this != &tree_node);

    const TTreeType* node_ptr = GetParent();

    while (node_ptr) {
        if (node_ptr == &tree_node)
            return true;
        node_ptr = node_ptr->GetParent();
    }
    return false;
}


template<class TValue, class TKeyGetter>
void CTreeNode<TValue, TKeyGetter>::FindNodes(const TKeyList& node_path,
                                              TNodeList*      res)
{
    TTreeType* tr = this;

    ITERATE(typename TKeyList, sit, node_path) {
        const TKeyType& key = *sit;
        bool sub_level_found = false;

        TNodeList_I it = tr->SubNodeBegin();
        TNodeList_I it_end = tr->SubNodeEnd();

        for (; it != it_end; ++it) {
            TTreeType* node = *it;
            if (node->GetKey() == key) {
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



template<class TValue, class TKeyGetter>
void CTreeNode<TValue, TKeyGetter>::FindNodes(const TKeyList& node_path,
                                              TConstNodeList* res) const
{
    const TTreeType* tr = this;

    ITERATE(typename TKeyList, sit, node_path) {
        const TKeyType& key = *sit;
        bool sub_level_found = false;

        TNodeList_CI it = tr->SubNodeBegin();
        TNodeList_CI it_end = tr->SubNodeEnd();

        for (; it != it_end; ++it) {
            const TTreeType* node = *it;
            if (node->GetKey() == key) {
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

    res->push_back(tr);
}

template<class TValue, class TKeyGetter>
const typename CTreeNode<TValue, TKeyGetter>::TTreeType*
CTreeNode<TValue, TKeyGetter>::FindSubNode(const TKeyType& key) const
{
    TNodeList_CI it = SubNodeBegin();
    TNodeList_CI it_end = SubNodeEnd();

    for(; it != it_end; ++it) {
        if ((*it)->GetKey() == key) {
            return *it;
        }
    }
    return 0;
}

template<class TValue, class TKeyGetter>
const typename CTreeNode<TValue, TKeyGetter>::TTreeType*
CTreeNode<TValue, TKeyGetter>::FindNode(const TKeyType& key,
                                        TNodeSearchMode sflag) const
{
    const TTreeType* ret = 0;
    if (sflag & eImmediateSubNodes) {
         ret = FindSubNode(key);
    }

    if (!ret && (sflag & eAllUpperSubNodes)) {
        const TTreeType* parent = GetParent();
        for (; parent; parent = parent->GetParent()) {
            ret = parent->FindSubNode(key);
            if (ret) {
                return ret;
            }
        }
    }

    if (!ret && (sflag & eTopLevelNodes)) {
        const TTreeType* root = GetRoot();
        if (root != this) {
            ret = root->FindSubNode(key);
        }
    }
    return ret;
}

/* @} */

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.46  2006/11/22 14:45:12  didenko
 * a node's parent should be cleared before the node is deleted
 *
 * Revision 1.45  2006/10/24 19:11:55  ivanov
 * Cosmetics: replaced tabulation with spaces
 *
 * Revision 1.44  2006/05/01 13:54:22  jcherry
 * Corrected return type of CTree::operator->
 *
 * Revision 1.43  2005/12/23 15:13:41  yazhuk
 * Added reverse iterators
 *
 * Revision 1.42  2005/11/08 20:29:31  grichenk
 * Simplified CPairNodeKeyGetter template arguments
 *
 * Revision 1.41  2005/10/27 16:48:48  grichenk
 * Redesigned CTreeNode (added search methods),
 * removed CPairTreeNode.
 *
 * Revision 1.40  2004/10/21 13:27:46  kuznets
 * Improved documentation
 *
 * Revision 1.39  2004/08/18 12:11:56  kuznets
 * Added type castings (compatibility fix for algorithms)
 *
 * Revision 1.38  2004/07/29 16:57:51  kuznets
 * +CTreePairNode::FindNode
 *
 * Revision 1.37  2004/07/29 13:14:08  kuznets
 * +CTreePairNode::AddNode()
 *
 * Revision 1.36  2004/07/22 17:20:20  ucko
 * Qualify dependent names in FindSubNode to fix GCC 3.4 build.
 *
 * Revision 1.35  2004/07/22 16:30:38  kuznets
 * +CTreePairNode::FindSubNode
 *
 * Revision 1.34  2004/06/15 13:01:14  ckenny
 * added breadth-first traversal algo
 *
 * Revision 1.33  2004/04/26 14:47:51  ucko
 * Qualify dependent names with "this->" as needed (by GCC 3.4).
 *
 * Revision 1.32  2004/04/19 16:00:14  kuznets
 * Algorithms migrated to <algo/tree>
 *
 * Revision 1.31  2004/04/12 15:17:46  vasilche
 * Added missing typename keyword.
 *
 * Revision 1.30  2004/04/12 13:28:25  kuznets
 * + MoveSubnodes
 *
 * Revision 1.29  2004/04/09 15:27:30  gorelenk
 * Added missed 'typename' (s) .
 *
 * Revision 1.28  2004/04/09 14:25:56  kuznets
 * + TreeReRoot, improved internal data integrity diagnostics
 *
 * Revision 1.27  2004/04/08 18:34:19  kuznets
 * attached code to a doxygen group
 * +TreePrinting template (for debug purposes)
 *
 * Revision 1.26  2004/04/08 12:22:56  kuznets
 * Fixed compilation warnings
 *
 * Revision 1.25  2004/04/08 11:47:46  kuznets
 * + TreeFindCommonParent, + TreeTraceToRoot
 *
 * Revision 1.24  2004/04/06 15:53:16  kuznets
 * Minor correction in the comments
 *
 * Revision 1.23  2004/03/10 15:59:33  kuznets
 * TreeDepthFirstTraverse fixed bug in tree iteration logic
 * (corner case for empty tree)
 *
 * Revision 1.22  2004/02/17 19:07:27  kuznets
 * GCC warning fix
 *
 * Revision 1.21  2004/01/29 20:03:17  jcherry
 * Added default value for AddNode(const TValue& val)
 *
 * Revision 1.20  2004/01/15 20:05:53  yazhuk
 * Added return falue to operator=
 *
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
