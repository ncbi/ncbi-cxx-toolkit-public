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

template <class TValue> class CTreeNWay
{
public:
    typedef TValue                     TValueType;
    typedef CTreeNWay<TValue>          TTreeType;
    typedef list<TTreeType*>           TNodeList;
    typedef typename TNodeList::iterator        TNodeList_I;
    typedef typename TNodeList::const_iterator  TNodeList_CI;

    /// Tree node construction
    ///
    /// @param
    ///   value - node value
    CTreeNWay(const TValue& value = TValue());
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
    CTreeNWay<TValue>* AddNode(const TValue& val);


    /// Insert new subnode before the specified location in the subnode list
    ///
    /// @param
    ///    it subnote iterator idicates the location of the new subtree
    /// @param 
    ///    subnode subtree pointer
    void InsertNode(TNodeList_I it, TTreeType* subnode);

protected:
    void CopyFrom(const TTreeType& tree);
    void SetParent(TTreeType* parent_node) { m_Parent = parent_node; }

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


template <class TId, class TValue> class CTreePairNWay
    : public CTreeNWay< CTreePair<TId, TValue> >
{
public:
    typedef CTreeNWay<CTreePair<TId, TValue> >   TParent;
    typedef CTreePair<TId, TValue>               TTreePair;

public:

    CTreePairNWay(const TId& id = TId(), const TValue& value = V());
    CTreePairNWay(const CTreePairNWay<TId, TValue>& tr);
    CTreePairNWay<TId, TValue>& operator=(const CTreePairNWay<TId, TValue>& tr);
};


/////////////////////////////////////////////////////////////////////////////
//
//  CTreeNWay<TValue>
//

template<class TValue>
CTreeNWay<TValue>::CTreeNWay(const TValue& value)
: m_Parent(0),
  m_Value(value)
{}

template<class TValue>
CTreeNWay<TValue>::~CTreeNWay()
{
    ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNWay* node = *it;
        delete node;
    }
}

template<class TValue>
CTreeNWay<TValue>::CTreeNWay(const TTreeType& tree)
: m_Parent(0),
  m_Value(tree.m_Value)
{
    CopyFrom(tree);
}

template<class TValue>
CTreeNWay<TValue>& CTreeNWay<TValue>::operator=(const TTreeType& tree)
{
    NON_CONST_ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNWay* node = *it;
        delete node;
    }
    m_Nodes.clear();
    CopyFrom(tree);
}

template<class TValue>
void CTreeNWay<TValue>::CopyFrom(const TTreeType& tree)
{
    ITERATE(typename TNodeList, it, tree.m_Nodes) {
        const CTreeNWay* src_node = *it;
        CTreeNWay* new_node = new CTreeNWay(*src_node);
        AddNode(new_node);
    }
}

template<class TValue>
void CTreeNWay<TValue>::RemoveNode(TTreeType* subnode)
{
    NON_CONST_ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNWay* node = *it;
        if (node == subnode) {
            m_Nodes.erase(it);
            delete node;
            break;
        }
    }    
}

template<class TValue>
void CTreeNWay<TValue>::RemoveNode(TNodeList_I it)
{
    CTreeNWay* node = *it;
    m_Nodes.erase(it);
    delete node;
}


template<class TValue>
typename CTreeNWay<TValue>::TTreeType* 
CTreeNWay<TValue>::DetachNode(typename CTreeNWay<TValue>::TTreeType* subnode)
{
    NON_CONST_ITERATE(typename TNodeList, it, m_Nodes) {
        CTreeNWay* node = *it;
        if (node == subnode) {
            m_Nodes.erase(it);
            node->SetParent(0);
            return node;
        }
    }        
    return 0;
}


template<class TValue>
typename CTreeNWay<TValue>::TTreeType* 
CTreeNWay<TValue>::DetachNode(CTreeNWay<TValue>::TNodeList_I it)
{
    CTreeNWay* node = *it;
    m_Nodes.erase(it);
    node->SetParent(0);

    return node;
}


template<class TValue>
void CTreeNWay<TValue>::AddNode(CTreeNWay<TValue>::TTreeType* subnode)
{
    m_Nodes.push_back(subnode);
    subnode->SetParent(this);
}

template<class TValue>
CTreeNWay<TValue>* CTreeNWay<TValue>::AddNode(const TValue& val)
{
    TTreeType* subnode = new TTreeType(val);
    AddNode(subnode);
    return subnode;
}

template<class TValue>
void CTreeNWay<TValue>::InsertNode(TNodeList_I it,
                                   TTreeType* subnode)
{
    m_Nodes.insert(it, subnode);
    subnode->SetParent(this);
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTreePairNWay<TId, TValue>
//


template<class TId, class TValue>
CTreePairNWay<TId, TValue>::CTreePairNWay(const TId& id, const TValue& value)
: CTreePairNWay<TId, TValue>::TParent(TTreePair(id, value))
{}


template<class TId, class TValue>
CTreePairNWay<TId, TValue>::CTreePairNWay(const CTreePairNWay<TId, TValue>& tr)
: TParent(tr)
{}


template<class TId, class TValue>
CTreePairNWay<TId, TValue>& 
CTreePairNWay<TId, TValue>::operator=(const CTreePairNWay<TId, TValue>& tr)
{
    TParent::operator=(tr);
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
