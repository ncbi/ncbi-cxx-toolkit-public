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
#include <set>
#include <map>
#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CPriorityIterator;
class CTSE_Info;
class CDataSource;
class CDataLoader;
class CDataSource_ScopeInfo;

class NCBI_XOBJMGR_EXPORT CPriorityNode
{
public:
    typedef CDataSource_ScopeInfo TLeaf;

    CPriorityNode(void);
    CPriorityNode(TLeaf& leaf);

    CPriorityNode(const CPriorityNode& node);
    CPriorityNode& operator=(const CPriorityNode& node);

    typedef int TPriority;
    typedef CPriorityIterator iterator;
    typedef set<CPriorityNode> TPrioritySet;
    typedef map<TPriority, TPrioritySet> TPriorityMap;

    bool operator<(const CPriorityNode& node) const;

    // true if the node is a tree, not a leaf
    bool IsTree(void) const;
    bool IsLeaf(void) const;

    TLeaf& GetLeaf(void);
    const TLeaf& GetLeaf(void) const;
    TPriorityMap& GetTree(void);
    const TPriorityMap& GetTree(void) const;

    // Set node type to "tree"
    TPriorityMap& SetTree(void);
    // Set node type to "leaf"
    void SetLeaf(TLeaf& leaf);

    bool Insert(const CPriorityNode& node, TPriority priority);
    bool Insert(CDataSource& ds, TPriority priority);
    bool Erase(const TLeaf& leaf);
    bool IsEmpty(void) const;
    void Clear(void);

private:

    void x_CopySubTree(const CPriorityNode& node);

    auto_ptr<TPriorityMap> m_SubTree;
    CRef<TLeaf>            m_Leaf;
};


class CPriority_I
{
public:
    CPriority_I(void);
    CPriority_I(CPriorityNode& node);

    operator bool(void);
    CPriorityNode::TLeaf& operator*(void);
    CPriorityNode::TLeaf* operator->(void);

    CPriority_I& operator++(void);

private:
    CPriority_I(const CPriority_I&);
    CPriority_I& operator= (const CPriority_I&);

    friend class CPriorityNode;
    typedef CPriorityNode::TPriorityMap TPriorityMap;
    typedef TPriorityMap::iterator      TMap_I;
    typedef CPriorityNode::TPrioritySet TPrioritySet;
    typedef TPrioritySet::iterator      TSet_I;

    TPriorityMap*           m_Map;
    TMap_I                  m_Map_I;
    TSet_I                  m_Set_I;
    CPriorityNode*          m_Node;
    auto_ptr<CPriority_I>   m_Sub_I;
};


// CPriorityNode inline methods

inline
bool CPriorityNode::operator<(const CPriorityNode& node) const
{
    return (m_SubTree.get() < node.m_SubTree.get()) || (m_Leaf < node.m_Leaf);
}

inline
bool CPriorityNode::IsTree(void) const
{
    return m_SubTree.get() != 0;
}

inline
bool CPriorityNode::IsLeaf(void) const
{
    return m_Leaf;
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
CPriorityNode::TPriorityMap& CPriorityNode::GetTree(void)
{
    _ASSERT(IsTree());
    return *m_SubTree;
}

inline
const CPriorityNode::TPriorityMap& CPriorityNode::GetTree(void) const
{
    _ASSERT(IsTree());
    return *m_SubTree;
}

inline
bool CPriorityNode::IsEmpty(void) const
{
    return !IsLeaf()  &&  (!IsTree()  ||  m_SubTree->empty());
}


// CPriority_I inline methods

inline
CPriority_I::operator bool(void)
{
    return m_Node != 0;
}

inline
CPriorityNode::TLeaf& CPriority_I::operator*(void)
{
    _ASSERT(m_Node  &&  (m_Node->IsTree()  ||  m_Node->IsLeaf()));
    if (m_Sub_I.get() != 0) {
        return **m_Sub_I;
    }
    return m_Node->GetLeaf();
}

inline
CPriorityNode::TLeaf* CPriority_I::operator->(void)
{
    return &this->operator *();
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2003/06/19 18:23:45  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.6  2003/05/14 18:39:26  grichenk
* Simplified TSE caching and filtering in CScope, removed
* some obsolete members and functions.
*
* Revision 1.5  2003/05/06 18:54:08  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.4  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.3  2003/04/18 20:08:53  kans
* changed iterate to ITERATE
*
* Revision 1.2  2003/04/09 16:04:30  grichenk
* SDataSourceRec replaced with CPriorityNode
* Added CScope::AddScope(scope, priority) to allow scope nesting
*
* Revision 1.1  2003/03/11 14:14:49  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif // PRIORITY__HPP
