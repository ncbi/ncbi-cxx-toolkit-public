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
    typedef list<const TTreeType*>     TConstNodeList;
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


template <class TId, class TValue> class CTreePairNWay
    : public CTreeNWay< CTreePair<TId, TValue> >
{
public:
    typedef CTreeNWay<CTreePair<TId, TValue> >   TParent;
    typedef CTreePair<TId, TValue>               TTreePair;
    typedef CTreePairNWay<TId, TValue>           TPairTreeType;
    typedef list<TPairTreeType*>                 TPairTreeNodeList;
    typedef list<const TPairTreeType*>           TConstPairTreeNodeList;

public:

    CTreePairNWay(const TId& id = TId(), const TValue& value = V());
    CTreePairNWay(const CTreePairNWay<TId, TValue>& tr);
    CTreePairNWay<TId, TValue>& operator=(const CTreePairNWay<TId, TValue>& tr);


    /// Add new subnode whose value is (a copy of) val
    ///
    /// @param 
    ///    val value reference
    ///
    /// @return pointer to new subtree
    CTreePairNWay<TId, TValue>* AddNode(const TId& id, const TValue& value);

    const TValue& GetValue() const { return m_Value.value; }

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
CTreeNWay<TValue>::DetachNode(typename CTreeNWay<TValue>::TNodeList_I it)
{
    CTreeNWay* node = *it;
    m_Nodes.erase(it);
    node->SetParent(0);

    return node;
}


template<class TValue>
void CTreeNWay<TValue>::AddNode(typename CTreeNWay<TValue>::TTreeType* subnode)
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
: //CTreePairNWay<TId, TValue>::TParent(TTreePair(id, value))
  TParent(TTreePair(id, value))
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

template<class TId, class TValue>
CTreePairNWay<TId, TValue>*
CTreePairNWay<TId, TValue>::AddNode(const TId& id, const TValue& value)
{
    return (CTreePairNWay<TId, TValue>*)TParent::AddNode(TTreePair(id, value));
}

template<class TId, class TValue>
void CTreePairNWay<TId, TValue>::FindNodes(const list<TId>& node_path, 
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

            if (node_pair.id == search_id) {  // value found
                tr = node;
                sub_level_found = true;
                break;
            }
        } // for it

        if (!sub_level_found) {
            break;
        }
        sub_level_found = false;

    } // ITERATE

    res.push_back(tr);
}


template<class TId, class TValue>
void CTreePairNWay<TId, TValue>::FindNodes(const list<TId>&         node_path, 
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

            if (node_pair.id == search_id) {  // value found
                tr = node;
                sub_level_found = true;
                break;
            }
        } // for it

        if (!sub_level_found) {
            break;
        }
        sub_level_found = false;

    } // ITERATE

    res->push_back((TPairTreeType*)tr);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2004/01/12 15:26:22  kuznets
 * Fixed various compilation warnings (GCC & WorkShop)
 *
 * Revision 1.8  2004/01/12 15:01:58  kuznets
 * +CTreePairNWay::FindNodes
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
