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

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
///    Bi-directionaly linked N way tree.
///

template <class V> class CTreeNWay
{
public:
    typedef V                          value_type;
    typedef CTreeNWay<V>               TTreeType;
    typedef list<TTreeType*>           TNodeList;
    typedef TNodeList::iterator        nodelist_iterator;
    typedef TNodeList::const_iterator  const_nodelist_iterator;

    /// Tree node construction
    ///
    /// @param
    ///   value - node value
    CTreeNWay(const V& value = V());
    ~CTreeNWay();

    CTreeNWay(const TTreeType& tree);
    CTreeNWay& operator =(const TTreeType& tree);

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
    const_nodelist_iterator SubNodeBegin() const { return m_Nodes.begin(); }

    /// Return first iterator on subnode list
    nodelist_iterator SubNodeBegin() { return m_Nodes.begin(); }

    /// Return last const iterator on subnode list
    const_nodelist_iterator SubNodeEnd() const { return m_Nodes.end(); }

    /// Return last iterator on subnode list
    nodelist_iterator SubNodeEnd() { return m_Nodes.end(); }

    /// Return node's value
    const V& GetValue() const { return m_Value; }

    /// Return node's value
    V& GetValue() { return m_Value; }

    /// Set value for the node
    void SetValue(const V& value) { m_Value = value; }

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
    void RemoveNode(nodelist_iterator it);

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
    TTreeType* DetachNode(nodelist_iterator it);

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
    CTreeNWay<V>* AddNode(const V& val);


    /// Insert new subnode before the specified location in the subnode list
    ///
    /// @param
    ///    it subnote iterator idicates the location of the new subtree
    /// @param 
    ///    subnode subtree pointer
    void InsertNode(nodelist_iterator it, TTreeType* subnode);

protected:
    void CopyFrom(const TTreeType& tree);
    void SetParent(TTreeType* parent_node) { m_Parent = parent_node; }

protected:
    TTreeType*         m_Parent; ///< Pointer on the parent node
    TNodeList          m_Nodes;  ///< List of dependent nodes
    V                  m_Value;  ///< Node value
};

/////////////////////////////////////////////////////////////////////////////
//
//  CTreeNWay<V>
//

template<class V>
CTreeNWay<V>::CTreeNWay(const V& value)
: m_Parent(0),
  m_Value(value)
{}

template<class V>
CTreeNWay<V>::~CTreeNWay()
{
    ITERATE(TNodeList, it, m_Nodes) {
        CTreeNWay* node = *it;
        delete node;
    }
}

template<class V>
CTreeNWay<V>::CTreeNWay(const TTreeType& tree)
{
    CopyFrom(tree);
}

template<class V>
CTreeNWay<V>& CTreeNWay<V>::operator=(const TTreeType& tree)
{
    ITERATE(TNodeList, it, m_Nodes) {
        CTreeNWay* node = *it;
        delete node;
    }
    m_Nodes.clear();
    CopyFrom(tree);
}

template<class V>
void CTreeNWay<V>::CopyFrom(const TTreeType& tree)
{
    ITERATE(TNodeList, it, tree.m_Nodes) {
        CTreeNWay* src_node = *it;
        CTreeNWay* new_node = new CTreeNWay(*src_node);
        AddNode(new_node);
    }
}

template<class V>
void CTreeNWay<V>::RemoveNode(TTreeType* subnode)
{
    ITERATE(TNodeList, it, m_Nodes) {
        CTreeNWay* node = *it;
        if (node == subnode) {
            m_Nodes.erase(it);
            delete node;
            break;
        }
    }    
}

template<class V>
void CTreeNWay<V>::RemoveNode(nodelist_iterator it)
{
    CTreeNWay* node = *it;
    m_Nodes.erase(it);
    delete node;
}


template<class V>
CTreeNWay<V>::TTreeType* CTreeNWay<V>::DetachNode(TTreeType* subnode)
{
    ITERATE(TNodeList, it, m_Nodes) {
        CTreeNWay* node = *it;
        if (node == subnode) {
            m_Nodes.erase(it);
            node->SetParent(0);
            return node;
        }
    }        
    return 0;
}


template<class V>
CTreeNWay<V>::TTreeType* CTreeNWay<V>::DetachNode(nodelist_iterator it)
{
    CTreeNWay* node = *it;
    m_Nodes.erase(it);
    node->SetParent(0);

    return node;
}


template<class V>
void CTreeNWay<V>::AddNode(TTreeType* subnode)
{
    m_Nodes.push_back(subnode);
    subnode->SetParent(this);
}


template<class V>
CTreeNWay<V>* CTreeNWay<V>::AddNode(const V& val)
{
    TTreeType* subnode = new TTreeType(val);
    AddNode(subnode);
    return subnode;
}


template<class V>
void CTreeNWay<V>::InsertNode(nodelist_iterator it,
                              TTreeType* subnode)
{
    m_Nodes.insert(it, subnode);
    subnode->SetParent(this);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
