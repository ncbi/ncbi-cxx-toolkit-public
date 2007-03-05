#ifndef PRIORITY__HPP
#define PRIORITY__HPP

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
*           Aleksey Grichenko
*
* File Description:
*           Priority record for CObjectManager and CScope
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiobj.hpp>
#include <map>
#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CPriority_I;
class CTSE_Info;
class CScope_Impl;
class CDataSource;
class CDataSource_ScopeInfo;

class CPriorityTree;
class CPriorityNode;

class NCBI_XOBJMGR_EXPORT CPriorityNode
{
public:
    typedef CDataSource_ScopeInfo TLeaf;

    CPriorityNode(void);
    explicit CPriorityNode(TLeaf& leaf);
    explicit CPriorityNode(const CPriorityTree& tree);
    CPriorityNode(CScope_Impl& scope, const CPriorityNode& node);

    typedef int TPriority;
    typedef multimap<TPriority, CPriorityNode> TPriorityMap;

    // true if the node is a tree, not a leaf
    bool IsTree(void) const;
    bool IsLeaf(void) const;

    TLeaf& GetLeaf(void);
    const TLeaf& GetLeaf(void) const;
    CPriorityTree& GetTree(void);
    const CPriorityTree& GetTree(void) const;

    // Set node type to "tree"
    CPriorityTree& SetTree(void);
    // Set node type to "leaf"
    void SetLeaf(TLeaf& leaf);

    size_t Erase(const TLeaf& leaf);
    bool IsEmpty(void) const; // true if node is null or empty tree
    void Clear(void);

private:

    CRef<CPriorityTree> m_SubTree;
    CRef<TLeaf>         m_Leaf;
};


class NCBI_XOBJMGR_EXPORT CPriorityTree : public CObject
{
public:
    typedef CDataSource_ScopeInfo TLeaf;

    CPriorityTree(void);
    CPriorityTree(CScope_Impl& scope, const CPriorityTree& node);
    ~CPriorityTree(void);

    typedef CPriorityNode::TPriority TPriority;
    typedef multimap<TPriority, CPriorityNode> TPriorityMap;

    TPriorityMap& GetTree(void);
    const TPriorityMap& GetTree(void) const;

    bool Insert(const CPriorityNode& node, TPriority priority);
    bool Insert(const CPriorityTree& tree, TPriority priority);
    bool Insert(TLeaf& leaf, TPriority priority);

    size_t Erase(const TLeaf& leaf);

    bool IsEmpty(void) const; // true if map is empty
    bool HasSeveralNodes(void); // true if tree has more than one node
    void Clear(void);

private:

    TPriorityMap m_Map;
};


class NCBI_XOBJMGR_EXPORT CPriority_I
{
public:
    CPriority_I(void);
    CPriority_I(CPriorityTree& tree);

    typedef CPriorityNode::TLeaf TLeaf;
    typedef TLeaf value_type;

    DECLARE_OPERATOR_BOOL_PTR(m_Node);

    value_type& operator*(void) const;
    value_type* operator->(void) const;

    const CPriority_I& operator++(void);

    const CPriority_I& InsertBefore(TLeaf& leaf);

private:
    CPriority_I(const CPriority_I&);
    CPriority_I& operator= (const CPriority_I&);

    typedef CPriorityTree::TPriorityMap TPriorityMap;
    typedef TPriorityMap::iterator      TMap_I;

    TPriorityMap*           m_Map;
    TMap_I                  m_Map_I;
    CPriorityNode*          m_Node;
    auto_ptr<CPriority_I>   m_Sub_I;
};


// CPriorityTree inline methods

inline
CPriorityTree::TPriorityMap& CPriorityTree::GetTree(void)
{
    return m_Map;
}

inline
const CPriorityTree::TPriorityMap& CPriorityTree::GetTree(void) const
{
    return m_Map;
}

inline
bool CPriorityTree::IsEmpty(void) const
{
    return m_Map.empty();
}


// CPriorityNode inline methods

inline
bool CPriorityNode::IsTree(void) const
{
    return m_SubTree.NotEmpty();
}

inline
bool CPriorityNode::IsLeaf(void) const
{
    return m_Leaf.NotEmpty();
}

inline
CDataSource_ScopeInfo& CPriorityNode::GetLeaf(void)
{
    _ASSERT(IsLeaf());
    return *m_Leaf;
}

inline
const CDataSource_ScopeInfo& CPriorityNode::GetLeaf(void) const
{
    _ASSERT(IsLeaf());
    return *m_Leaf;
}

inline
CPriorityTree& CPriorityNode::GetTree(void)
{
    _ASSERT(IsTree());
    return *m_SubTree;
}

inline
const CPriorityTree& CPriorityNode::GetTree(void) const
{
    _ASSERT(IsTree());
    return *m_SubTree;
}

inline
bool CPriorityNode::IsEmpty(void) const
{
    return !IsLeaf()  &&  (!IsTree()  ||  m_SubTree->IsEmpty());
}


// CPriority_I inline methods

inline
CPriority_I::value_type& CPriority_I::operator*(void) const
{
    _ASSERT(m_Node  &&  (m_Node->IsTree()  ||  m_Node->IsLeaf()));
    if (m_Sub_I.get()) {
        return **m_Sub_I;
    }
    return m_Node->GetLeaf();
}

inline
CPriority_I::value_type* CPriority_I::operator->(void) const
{
    return &this->operator *();
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // PRIORITY__HPP
