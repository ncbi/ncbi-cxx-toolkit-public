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
#include <set>
#include <map>
#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CDataSource;

class CPriorityIterator;

class CPriorityNode {
public:
    CPriorityNode(void);
    CPriorityNode(CDataSource& ds);

    CPriorityNode(const CPriorityNode& node);
    CPriorityNode& operator=(const CPriorityNode& node);

    typedef int TPriority;
    typedef CPriorityIterator iterator;
    typedef set<CPriorityNode> TPrioritySet;
    typedef map<TPriority, TPrioritySet> TPriorityMap;

    bool operator==(const CPriorityNode& node) const;
    bool operator<(const CPriorityNode& node) const;

    // true if the node is a tree, not a leaf
    bool IsTree(void) const;
    bool IsDataSource(void) const;
    CDataSource& GetDataSource(void) const;
    TPriorityMap& GetTree(void) const;
    // Set node type to "tree"
    TPriorityMap& SetTree(void);
    void SetDataSource(CDataSource& ds);

    bool Insert(const CPriorityNode& node, TPriority priority);
    bool Erase(const CDataSource& ds);
    bool IsEmpty(void) const;
    void Clear(void);

private:

    void x_CopySubTree(const CPriorityNode& node);

    auto_ptr<TPriorityMap> m_SubTree;
    CDataSource* m_DataSource;
};


class CPriority_I
{
public:
    CPriority_I(void);
    CPriority_I(const CPriorityNode& node);

    operator bool(void);
    CDataSource& operator*(void);
    CDataSource* operator->(void);

    CPriority_I& operator++(void);

    typedef vector<CPriorityNode::TPriority> TPriorityVector;
    void GetPriorityVector(TPriorityVector& v) const;

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
    const CPriorityNode*    m_Node;
    auto_ptr<CPriority_I>   m_Sub_I;
};


// CPriorityNode inline methods

inline
CPriorityNode::CPriorityNode(void)
    : m_SubTree(0), m_DataSource(0)
{
}

inline
CPriorityNode::CPriorityNode(CDataSource& ds)
    : m_SubTree(0), m_DataSource(&ds)
{
}

inline
CPriorityNode::CPriorityNode(const CPriorityNode& node)
    : m_SubTree(0), m_DataSource(node.m_DataSource)
{
    if ( node.IsTree() )
        x_CopySubTree(node);
}

inline
CPriorityNode& CPriorityNode::operator=(const CPriorityNode& node)
{
    if (this != &node) {
        m_DataSource = node.m_DataSource;
        if ( node.IsTree() ) {
            x_CopySubTree(node);
        }
        else {
            m_SubTree.reset();
        }
    }
    return *this;
}

inline
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

inline
bool CPriorityNode::operator==(const CPriorityNode& node) const
{
    return m_SubTree.get() == node.m_SubTree.get()
        &&  m_DataSource == node.m_DataSource;
}

inline
bool CPriorityNode::operator<(const CPriorityNode& node) const
{
    return IsTree() ?
        (m_SubTree.get() < node.m_SubTree.get()) :
        (m_DataSource < node.m_DataSource);
}

inline
bool CPriorityNode::IsTree(void) const
{
    return m_SubTree.get() != 0;
}

inline
bool CPriorityNode::IsDataSource(void) const
{
    return m_DataSource != 0;
}

inline
CDataSource& CPriorityNode::GetDataSource(void) const
{
    _ASSERT(m_DataSource);
    return *m_DataSource;
}

inline
CPriorityNode::TPriorityMap& CPriorityNode::GetTree(void) const
{
    _ASSERT(IsTree());
    return *m_SubTree;
}

inline
CPriorityNode::TPriorityMap& CPriorityNode::SetTree(void)
{
    m_DataSource = 0;
    if (m_SubTree.get() == 0)
        m_SubTree.reset(new TPriorityMap);
    return *m_SubTree;
}

inline
void CPriorityNode::SetDataSource(CDataSource& ds)
{
    m_SubTree.reset();
    m_DataSource = &ds;
}

inline
bool CPriorityNode::Insert(const CPriorityNode& node, TPriority priority)
{
    _ASSERT(IsTree());
    return (*m_SubTree)[priority].insert(node).second;
}

inline
bool CPriorityNode::Erase(const CDataSource& ds)
{
    if ( IsTree() ) {
        TPrioritySet::iterator sit;
        TPriorityMap::iterator mit;
        for (mit = m_SubTree->begin(); mit != m_SubTree->end(); ++mit) {
            for (sit = mit->second.begin(); sit != mit->second.end(); ++sit) {
                if ( const_cast<CPriorityNode&>(*sit).Erase(ds) )
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
    else if (m_DataSource == &ds) {
        m_DataSource = 0;
        return true;
    }
    return false;
}

inline
bool CPriorityNode::IsEmpty(void) const
{
    return m_DataSource == 0  &&
        (m_SubTree.get() == 0  ||  m_SubTree->empty());
}


inline
void CPriorityNode::Clear(void)
{
    m_DataSource = 0;
    if (m_SubTree.get() != 0) {
        m_SubTree->clear();
    }
}


// CPriority_I inline methods

inline
CPriority_I::CPriority_I(void)
    : m_Map(0), m_Node(0), m_Sub_I(0)
{
}

inline
CPriority_I::CPriority_I(const CPriorityNode& node)
    : m_Map(0), m_Node(0), m_Sub_I(0)
{
    if ( node.IsTree() ) {
        m_Map = &node.GetTree();
        m_Map_I = m_Map->begin();
        if (m_Map_I != m_Map->end()) {
            m_Set_I = m_Map_I->second.begin();
            if (m_Set_I != m_Map_I->second.end()) {
                m_Node = &(*m_Set_I);
                m_Sub_I.reset(new CPriority_I(*m_Node));
            }
        }
    }
    else if ( node.IsDataSource() ) {
        m_Node = &node;
    }
}

inline
CPriority_I::operator bool(void)
{
    return m_Node != 0;
}

inline
CDataSource& CPriority_I::operator*(void)
{
    _ASSERT(m_Node  &&  (m_Node->IsTree()  ||  m_Node->IsDataSource()));
    if (m_Sub_I.get() != 0) {
        return **m_Sub_I;
    }
    return m_Node->GetDataSource();
}

inline
CDataSource* CPriority_I::operator->(void)
{
    return &this->operator *();
}

inline
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
            m_Node = &(*m_Set_I);
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

inline
void CPriority_I::GetPriorityVector(TPriorityVector& v) const
{
    _ASSERT(m_Node  &&  (m_Node->IsTree()  ||  m_Node->IsDataSource()));
    if (m_Map  &&  m_Map_I != m_Map->end()) {
        v.push_back(m_Map_I->first);
        if ( m_Sub_I.get() != 0 ) {
            m_Sub_I->GetPriorityVector(v);
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
