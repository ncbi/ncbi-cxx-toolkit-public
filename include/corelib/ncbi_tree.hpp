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

template <class TValue> 
class CTreeNode
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

    /// Get the topmost node 
    ///
    /// @return global parent of the current node, this if it is a top
    /// of the tree
    const TTreeType* GetRoot() const;

    /// Get the topmost node 
    ///
    /// @return global parent of the current node, this if it is a top
    /// of the tree
    TTreeType* GetRoot();


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
    CTreeNode<TValue>* AddNode(const TValue& val = TValue());

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

    TId GetId() const { return id; }
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
    typedef typename TParent::TValueType         TTreeValueType;
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

    void AddNode(CTreePairNode<TId, TValue>* node) { TParent::AddNode(node); }

    const CTreePairNode<TId, TValue>* GetRoot() const 
    { return (const CTreePairNode<TId, TValue>*)TParent::GetRoot(); }

    CTreePairNode<TId, TValue>* GetRoot() 
    { return (CTreePairNode<TId, TValue>*)TParent::GetRoot(); }

    const CTreePairNode<TId, TValue>* GetParent() const 
    { return (const CTreePairNode<TId, TValue>*)TParent::GetParent(); }

    CTreePairNode<TId, TValue>* GetParent() 
    { return (CTreePairNode<TId, TValue>*)TParent::GetParent(); }

    /// Return TParent::TValueType 
    /// (tree node value in terms of the upper level tree)
    const TTreeValueType& GetTreeValue() const { return this->m_Value; }

    /// Return TParent::TValueType 
    /// (tree node value in terms of the upper level tree)
    TTreeValueType& GetTreeValue() { return this->m_Value; }

    /// Return node's value
    const TValueType& GetValue() const { return this->m_Value.value; }

    /// Return node's id
    const TIdType& GetId() const { return this->m_Value.id; }

    /// Return node's value
    TValueType& GetValue() { return this->m_Value.value; }

    /// Return node's id
    TIdType& GetId() { return this->m_Value.id; }

    /// Set value for the node
    void SetValue(const TValue& value) { this->m_Value.value = value; }

    /// Set id for the node
    void SetId(const TId& id) { this->m_Value.id = id; }

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
    void FindNodes(const list<TId>&        node_path, 
                   TConstPairTreeNodeList* res) const;

    /// Non recursive linear scan of all subnodes, with id comparison
    ///
    /// @return SubNode pointer or NULL
    ///
    /// If return value is not NULL Id and value can be received by
    /// FindSubNode(...)->GetId(), FindSubNode(...)->GetValue()
    const TPairTreeType* FindSubNode(const TId& id) const;

    /// Parameters for Id node search
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
    const TPairTreeType* FindNode(const TId&      id, 
                                  TNodeSearchMode sflag = eImmediateAndTop) const;

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
/// Algorithm calls the visitor function on the way back so it revisits
/// some tree nodes. It is possible to implement both variants of tree 
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

  if(stop_scan)
    return func;

  TTreeNode* tr = &tree_node;
  
  typedef typename TTreeNode::TNodeList_I TTreeNodeIterator;

  TTreeNodeIterator it = tr->SubNodeBegin();
  TTreeNodeIterator it_end = tr->SubNodeEnd();

  if(it == it_end)
    return func;

  queue<TTreeNodeIterator> tree_queue;

  while(it != it_end) 
    tree_queue.push(it++);
 
  while(!tree_queue.empty()) {

    it = tree_queue.front(); // get oldest node on queue
    tr = *it;
    tree_queue.pop(); // take oldest node off
    stop_scan = eTreeTraverse;
    if(tr) {
      stop_scan = func(*tr);
      switch(stop_scan) {
        case eTreeTraverseStop:
	  return func;
        case eTreeTraverse:
        case eTreeTraverseStepOver:
	  break;
      } // end switch
    } // end if
    // add children (if any) of node to queue
    if(stop_scan != eTreeTraverseStepOver && !tr->IsLeaf()) { 
      it = tr->SubNodeBegin();
      it_end = tr->SubNodeEnd();
      while(it != it_end)
	tree_queue.push(it++);
    } // end if

  } // end while

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
    _ASSERT(m_Parent == 0);
    ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNode* node = *it;
        node->m_Parent = 0;
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
    return *this;
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
            node->m_Parent = 0;
            delete node;
            break;
        }
    }    
}

template<class TValue>
void CTreeNode<TValue>::RemoveNode(TNodeList_I it)
{
    CTreeNode* node = *it;
    node->m_Parent = 0;
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
    _ASSERT(subnode != this);
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
void CTreeNode<TValue>::MoveSubnodes(TTreeType* src_tree_node)
{
    _ASSERT(!IsParent(*src_tree_node));
    TNodeList& src_nodes = src_tree_node->m_Nodes;
    ITERATE(typename TNodeList, it, src_nodes) {
        AddNode(*it);
    }
    src_nodes.clear();
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

template<class TValue>
const typename CTreeNode<TValue>::TTreeType* CTreeNode<TValue>::GetRoot() const
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

template<class TValue>
typename CTreeNode<TValue>::TTreeType* CTreeNode<TValue>::GetRoot()
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

template<class TValue>
bool CTreeNode<TValue>::IsParent(const TTreeType& tree_node) const
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

template<class TId, class TValue>
const CTreePairNode<TId, TValue>*
CTreePairNode<TId, TValue>::FindSubNode(const TId& id) const
{
    typename TParent::TNodeList_CI it = this->SubNodeBegin();
    typename TParent::TNodeList_CI it_end = this->SubNodeEnd();

    for(; it != it_end; ++it) {
        const TTreeValueType& v = (*it)->GetValue();
        if (v.id == id) {
            const TParent* pt = *it;
            return (CTreePairNode<TId, TValue>*)(pt);
        }
    }
    return 0;
}

template<class TId, class TValue>
const CTreePairNode<TId, TValue>*
CTreePairNode<TId, TValue>::FindNode(const TId&      id,
                                     TNodeSearchMode sflag) const
{
    const TPairTreeType* ret = 0;
    if (sflag & eImmediateSubNodes) {
         ret = FindSubNode(id);
    }

    if (!ret && (sflag & eAllUpperSubNodes)) {
        const TPairTreeType* parent = GetParent();
        for (;parent; parent = parent->GetParent()) {
            ret = parent->FindSubNode(id);
            if (ret)
                return ret;
        }
    }

    if (!ret && (sflag & eTopLevelNodes)) {
        const TPairTreeType* root = GetRoot();
        if (root != this) {
            ret = root->FindSubNode(id);
        }
    }
    return ret;
}

/* @} */

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
