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


class CPriority_I;
class CTSE_Info;
class CDataSource;
class CDataLoader;
struct CDataSource_ScopeInfo;

class CPriorityTree;
class CPriorityNode;

class NCBI_XOBJMGR_EXPORT CPriorityNode
{
public:
    typedef CDataSource_ScopeInfo TLeaf;

    CPriorityNode(void);
    explicit CPriorityNode(CDataSource& ds);
    explicit CPriorityNode(TLeaf& leaf);
    explicit CPriorityNode(const CPriorityTree& tree);

    CPriorityNode(const CPriorityNode& node);
    CPriorityNode& operator=(const CPriorityNode& node);

    typedef int TPriority;
    typedef CPriority_I iterator;
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

    //bool Insert(const CPriorityNode& node, TPriority priority);
    //bool Insert(CDataSource& ds, TPriority priority);
    bool Erase(const TLeaf& leaf);
    bool IsEmpty(void) const;
    void Clear(void);

private:

    void x_CopySubTree(const CPriorityNode& node);

    CRef<CPriorityTree> m_SubTree;
    CRef<TLeaf>         m_Leaf;
};


const CPriorityNode::TPriority kPriority_NotSet = -1;


class NCBI_XOBJMGR_EXPORT CPriorityTree : public CObject
{
public:
    typedef CDataSource_ScopeInfo TLeaf;

    CPriorityTree(void);
    CPriorityTree(const CPriorityTree& node);
    ~CPriorityTree(void);

    const CPriorityTree& operator=(const CPriorityTree& node);

    typedef CPriorityNode::TPriority TPriority;
    typedef CPriority_I iterator;
    typedef multimap<TPriority, CPriorityNode> TPriorityMap;

    TPriorityMap& GetTree(void);
    const TPriorityMap& GetTree(void) const;

    bool Insert(const CPriorityNode& node, TPriority priority);
    bool Insert(const CPriorityTree& tree, TPriority priority);
    bool Insert(CDataSource& ds, TPriority priority);

    bool Erase(const TLeaf& leaf);

    bool IsEmpty(void) const;
    void Clear(void);

private:

    void x_CopySubTree(const CPriorityNode& node);

    TPriorityMap m_Map;
};


class CPriority_I
{
public:
    CPriority_I(void);
    CPriority_I(CPriorityTree& tree);

    typedef CPriorityNode::TLeaf TLeaf;
    typedef TLeaf value_type;

    operator bool(void) const;
    value_type& operator*(void) const;
    value_type* operator->(void) const;

    const CPriority_I& operator++(void);

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
    return m_SubTree;
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
CPriority_I::operator bool(void) const
{
    return m_Node != 0;
}

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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2003/08/04 17:04:29  grichenk
* Added default data-source priority assignment.
* Added support for iterating all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.10  2003/06/30 19:12:40  vasilche
* Changed order of classes to make it compilable on MSVC.
*
* Revision 1.9  2003/06/30 18:42:09  vasilche
* CPriority_I made to use less memory allocations/deallocations.
*
* Revision 1.8  2003/06/19 18:34:07  vasilche
* Fixed compilation on Windows.
*
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
