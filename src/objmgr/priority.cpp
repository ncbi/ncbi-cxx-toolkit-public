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

#include <ncbi_pch.hpp>
#include <objmgr/impl/priority.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/scope_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// CPriorityTree methods

CPriorityTree::CPriorityTree(void)
{
}


CPriorityTree::~CPriorityTree(void)
{
}


CPriorityTree::CPriorityTree(CScope_Impl& scope, const CPriorityTree& tree)
{
    ITERATE ( TPriorityMap, it, tree.m_Map ) {
        m_Map.insert(TPriorityMap::value_type(
                         it->first, CPriorityNode(scope, it->second)));
    }
}


bool CPriorityTree::Insert(const CPriorityNode& node, TPriority priority)
{
    m_Map.insert(TPriorityMap::value_type(priority, node));
    return true;
}


bool CPriorityTree::Insert(const CPriorityTree& tree, TPriority priority)
{
    return Insert(CPriorityNode(tree), priority);
}


bool CPriorityTree::Insert(TLeaf& leaf,
                           TPriority priority)
{
    for ( TPriorityMap::const_iterator it = m_Map.lower_bound(priority);
          it != m_Map.end() && it->first == priority; ++it ) {
        if ( it->second.IsLeaf() &&
             &it->second.GetLeaf().GetDataSource() == &leaf.GetDataSource() ) {
            return false;
        }
    }
    return Insert(CPriorityNode(leaf), priority);
}


size_t CPriorityTree::Erase(const TLeaf& leaf)
{
    size_t count = 0;
    for ( TPriorityMap::iterator mit(m_Map.begin()); mit != m_Map.end(); ) {
        count += mit->second.Erase(leaf);
        if ( mit->second.IsEmpty() ) {
            m_Map.erase(mit++);
        }
        else {
            ++mit;
        }
    }
    return count;
}


void CPriorityTree::Clear(void)
{
    m_Map.clear();
}


bool CPriorityTree::HasSeveralNodes(void)
{
    CPriority_I iter(*this);
    return iter && ++iter;
}


// CPriorityNode methods


CPriorityNode::CPriorityNode(void)
{
}


CPriorityNode::CPriorityNode(TLeaf& leaf)
    : m_Leaf(&leaf)
{
}


CPriorityNode::CPriorityNode(const CPriorityTree& tree)
    : m_SubTree(new CPriorityTree(tree))
{
}


CPriorityNode::CPriorityNode(CScope_Impl& scope, const CPriorityNode& node)
{
    if ( node.IsTree() ) {
        m_SubTree = new CPriorityTree(scope, node.GetTree());
    }
    else if ( node.IsLeaf() ) {
        if ( !node.GetLeaf().CanBeEdited() ) { // shared -> use original DS
            CDataSource& ds =
                const_cast<CDataSource&>(node.GetLeaf().GetDataSource());
            m_Leaf = scope.x_GetDSInfo(ds);
        }
        else { // private -> copy DS content
            CRef<CDataSource> ds(new CDataSource);
            const CTSE_LockSet& blobs =
                node.GetLeaf().GetDataSource().GetStaticBlobs();
            ITERATE ( CTSE_LockSet, it, blobs ) {
                CRef<CTSE_Info> new_tse(new CTSE_Info(it->second));
                ds->AddStaticTSE(new_tse);
            }
            m_Leaf = scope.x_GetDSInfo(*ds);
        }
    }
}


CPriorityTree& CPriorityNode::SetTree(void)
{
    m_Leaf.Reset();
    if ( !m_SubTree )
        m_SubTree.Reset(new CPriorityTree());
    return *m_SubTree;
}


void CPriorityNode::SetLeaf(TLeaf& leaf)
{
    m_SubTree.Reset();
    m_Leaf.Reset(&leaf);
}


size_t CPriorityNode::Erase(const TLeaf& leaf)
{
    if ( IsTree() ) {
        return GetTree().Erase(leaf);
    }
    else if (m_Leaf == &leaf) {
        m_Leaf.Reset();
        return 1;
    }
    return 0;
}


void CPriorityNode::Clear(void)
{
    m_Leaf.Reset();
    if ( m_SubTree ) {
        m_SubTree->Clear();
    }
}


// CPriority_I methods


CPriority_I::CPriority_I(void)
    : m_Map(0), m_Node(0)
{
}


CPriority_I::CPriority_I(CPriorityTree& tree)
    : m_Map(&tree.GetTree()), m_Node(0)
{
    for ( m_Map_I = m_Map->begin(); m_Map_I != m_Map->end(); ++m_Map_I ) {
        m_Node = &m_Map_I->second;
        if ( m_Node->IsLeaf() )
            return;
        else if ( m_Node->IsTree() ) {
            m_Sub_I.reset(new CPriority_I(m_Node->GetTree()));
            if ( *m_Sub_I )
                return;
            m_Sub_I.reset();
        }
    }
    m_Node = 0;
}


const CPriority_I& CPriority_I::operator++(void)
{
    _ASSERT(m_Node && m_Map && m_Map_I != m_Map->end());
    if ( m_Sub_I.get() ) {
        // Try to increment sub-iterator
        if ( ++*m_Sub_I )
            return *this;
        m_Sub_I.reset();
    }
    // Current node is not a tree or the tree has been iterated to its end
    // Select next element in the set/map
    while ( ++m_Map_I != m_Map->end()) {
        m_Node = &m_Map_I->second;
        if ( m_Node->IsLeaf() )
            return *this;
        else if ( m_Node->IsTree() ) {
            m_Sub_I.reset(new CPriority_I(m_Node->GetTree()));
            if ( *m_Sub_I ) // found non-empty subtree
                return *this;
            m_Sub_I.reset();
        }
    }
    // No more valid nodes - reset node
    m_Node = 0;
    return *this;
}


const CPriority_I& CPriority_I::InsertBefore(TLeaf& leaf)
{
    _ASSERT(m_Node && m_Map && m_Map_I != m_Map->end());
    if ( m_Sub_I.get() ) {
        m_Sub_I->InsertBefore(leaf);
        return *this;
    }
    _ASSERT(m_Node->IsLeaf());
    CRef<TLeaf> old_leaf(&m_Node->GetLeaf());
    m_Node->SetTree().Insert(leaf, 0);
    m_Node->SetTree().Insert(*old_leaf, 1);
    m_Sub_I.reset(new CPriority_I(m_Node->GetTree()));
    _ASSERT(*m_Sub_I && &**m_Sub_I == &leaf);
    return *this;
}


END_SCOPE(objects)
END_NCBI_SCOPE
