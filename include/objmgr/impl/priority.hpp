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
    CPriorityNode(CScope_Impl& scope, CDataSource& ds);
    CPriorityNode(CScope_Impl& scope, const CPriorityNode& node);

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
    bool IsSingle(void) const;
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
    typedef CPriority_I iterator;
    typedef multimap<TPriority, CPriorityNode> TPriorityMap;

    TPriorityMap& GetTree(void);
    const TPriorityMap& GetTree(void) const;

    bool Insert(const CPriorityNode& node,
                TPriority priority);
    bool Insert(const CPriorityTree& tree,
                TPriority priority);
    bool Insert(CScope_Impl& scope,
                CDataSource& ds,
                TPriority priority);

    bool Erase(const TLeaf& leaf);

    bool IsEmpty(void) const;
    bool IsSingle(void) const;
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


inline
bool CPriorityTree::IsSingle(void) const
{
    return !IsEmpty()  &&
        m_Map.size() == 1  &&  m_Map.begin()->second.IsSingle();
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


inline
bool CPriorityNode::IsSingle(void) const
{
    return !IsEmpty()  &&  (IsLeaf()  ||  m_SubTree->IsSingle());
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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2005/03/29 19:21:56  jcherry
* Added export specifiers
*
* Revision 1.17  2005/03/29 16:04:50  grichenk
* Optimized annotation retrieval (reduced nuber of seq-ids checked)
*
* Revision 1.16  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.15  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.14  2004/12/22 15:56:25  vasilche
* Implemented deep copying of CPriorityTree.
*
* Revision 1.13  2004/02/19 17:16:35  vasilche
* Removed unused include.
*
* Revision 1.12  2003/09/30 16:22:01  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
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
