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
* Authors:
*           Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*           Priority record for CObjectManager and CScope
*
*/

#include <objmgr/impl/priority.hpp>

#include <objmgr/impl/scope_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// CPriorityNode methods


CPriorityNode::CPriorityNode(void)
    : m_SubTree(0), m_Leaf(0)
{
}


CPriorityNode::CPriorityNode(TLeaf& leaf)
    : m_SubTree(0), m_Leaf(&leaf)
{
}


CPriorityNode::CPriorityNode(const CPriorityNode& node)
    : m_SubTree(0), m_Leaf(node.m_Leaf)
{
    if ( node.IsTree() )
        x_CopySubTree(node);
}


CPriorityNode& CPriorityNode::operator=(const CPriorityNode& node)
{
    if (this != &node) {
        m_Leaf = node.m_Leaf;
        if ( node.IsTree() ) {
            x_CopySubTree(node);
        }
        else {
            m_SubTree.reset();
        }
    }
    return *this;
}


void CPriorityNode::x_CopySubTree(const CPriorityNode& node)
{
    m_SubTree.reset(new TPriorityMap);
    ITERATE(TPriorityMap, mit, node.GetTree()) {
        TPrioritySet& pset = (*m_SubTree)[mit->first];
        ITERATE(TPrioritySet, sit, mit->second) {
            pset.insert(*sit);
        }
    }
}


CPriorityNode::TPriorityMap& CPriorityNode::SetTree(void)
{
    m_Leaf = 0;
    if (m_SubTree.get() == 0)
        m_SubTree.reset(new TPriorityMap);
    return *m_SubTree;
}


void CPriorityNode::SetLeaf(TLeaf& leaf)
{
    m_SubTree.reset();
    m_Leaf = &leaf;
}


bool CPriorityNode::Insert(const CPriorityNode& node, TPriority priority)
{
    _ASSERT(IsTree());
    return (*m_SubTree)[priority].insert(node).second;
}


bool CPriorityNode::Insert(CDataSource& ds, TPriority priority)
{
    _ASSERT(IsTree());
    TPrioritySet& pset = (*m_SubTree)[priority];
    ITERATE ( TPrioritySet, it, pset ) {
        if ( it->IsLeaf() && &it->GetLeaf().GetDataSource() == &ds ) {
            return false;
        }
    }
    _VERIFY(pset.insert(*new CDataSource_ScopeInfo(ds)).second);
    return true;
}


bool CPriorityNode::Erase(const TLeaf& leaf)
{
    if ( IsTree() ) {
        TPrioritySet::iterator sit;
        TPriorityMap::iterator mit;
        for (mit = m_SubTree->begin(); mit != m_SubTree->end(); ++mit) {
            for (sit = mit->second.begin(); sit != mit->second.end(); ++sit) {
                if ( const_cast<CPriorityNode&>(*sit).Erase(leaf) )
                    break;
            }
            
        }
        if (mit != m_SubTree->end()  &&  sit != mit->second.end()) {
            // Found the node
            if ( sit->IsEmpty() ) {
                mit->second.erase(sit);
                if ( mit->second.empty() ) {
                    m_SubTree->erase(mit);
                }
            }
        }
    }
    else if (m_Leaf == &leaf) {
        m_Leaf = 0;
        return true;
    }
    return false;
}


void CPriorityNode::Clear(void)
{
    m_Leaf = 0;
    if (m_SubTree.get() != 0) {
        m_SubTree->clear();
    }
}


// CPriority_I methods


CPriority_I::CPriority_I(void)
    : m_Map(0), m_Node(0), m_Sub_I(0)
{
}


CPriority_I::CPriority_I(CPriorityNode& node)
    : m_Map(0), m_Node(0), m_Sub_I(0)
{
    if ( node.IsTree() ) {
        m_Map = &node.GetTree();
        m_Map_I = m_Map->begin();
        if (m_Map_I != m_Map->end()) {
            m_Set_I = m_Map_I->second.begin();
            if (m_Set_I != m_Map_I->second.end()) {
                m_Node = const_cast<CPriorityNode*>(&*m_Set_I);
                m_Sub_I.reset(new CPriority_I(*m_Node));
            }
        }
    }
    else if ( node.IsLeaf() ) {
        m_Node = &node;
    }
}


CPriority_I& CPriority_I::operator++(void)
{
    _ASSERT(m_Node);
    if (m_Sub_I.get() != 0) {
        // Try to increment sub-iterator
        ++(*m_Sub_I);
        if ( *m_Sub_I )
            return *this;
        m_Sub_I.reset();
    }
    // Current node is not a tree or the tree has been iterated to its end
    // Select next element in the set/map
    if ( !m_Map ) {
        // the node was of type "datasource", nothing else to iterate
        m_Node = 0;
        return *this;
    }
    ++m_Set_I;
    while (m_Map_I != m_Map->end()) {
        while (m_Set_I != m_Map_I->second.end()) {
            m_Node = const_cast<CPriorityNode*>(&*m_Set_I);
            m_Sub_I.reset(new CPriority_I(*m_Node));
            if ( *m_Sub_I ) {
                // found non-empty subtree
                return *this;
            }
            m_Sub_I.reset();
            ++m_Set_I;
        }
        ++m_Map_I;
        if (m_Map_I != m_Map->end()) {
            m_Set_I = m_Map_I->second.begin();
        }
    }
    // No more valid nodes - reset the iterator
    m_Node = 0;
    m_Map = 0;
    return *this;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/06/19 18:23:46  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* ===========================================================================
*/
