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
* Author: Eugene Vasilchenko
*
* File Description:
*   Implementation of interval search tree.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/itree.hpp>

BEGIN_NCBI_SCOPE

inline
CIntervalTree::coordinate_type CIntervalTree::GetMaxRootCoordinate(void) const
{
    coordinate_type max = m_Root.m_Key * 2;
    if ( max <= 0 )
        max = TTraits::GetMaxCoordinate();
    return max;
}

inline
CIntervalTree::coordinate_type CIntervalTree::GetNextRootKey(void) const
{
    coordinate_type nextKey = m_Root.m_Key * 2;
    _ASSERT(nextKey > 0);
    return nextKey;
}

void CIntervalTree::DoInsert(const interval_type& interval, TTreeMapI value)
{
    _ASSERT(TTraits::IsNormal(interval));

    // ensure our tree covers specified interval
    if ( interval.GetTo() > GetMaxRootCoordinate() ) {
        // insert one more level on top
        if ( m_Root.m_Left || m_Root.m_Right || m_Root.m_NodeIntervals ) {
            // non empty tree, insert new root node
            do {
                TTreeNode* newLeft = AllocNode();
                // copy root node contents
                *newLeft = m_Root;
                // fill new root
                m_Root.m_Key = GetNextRootKey();
                m_Root.m_Left = newLeft;
                m_Root.m_Right = 0;
                m_Root.m_NodeIntervals = 0;
            } while ( interval.GetTo() > GetMaxRootCoordinate() );
        }
        else {
            // empty tree, just recalculate root
            do {
                m_Root.m_Key = GetNextRootKey();
            } while ( interval.GetTo() > GetMaxRootCoordinate() );
        }
    }

    TTreeNode* node = &m_Root;
    coordinate_type nodeSize = m_Root.m_Key;
    for ( ;; ) {
        coordinate_type key = node->m_Key;
        nodeSize = (nodeSize + 1) / 2;

        TTreeNode** nextPtr;
        coordinate_type nextKeyOffset;

        if ( interval.GetFrom() > key  ) {
            nextPtr = &node->m_Right;
            nextKeyOffset = nodeSize;
        }
        else if ( interval.GetTo() < key ) {
            nextPtr = &node->m_Left;
            nextKeyOffset = -nodeSize;
        }
        else {
            // found our tile
            TTreeNodeInts* nodeIntervals = node->m_NodeIntervals;
            if ( !nodeIntervals )
                node->m_NodeIntervals = nodeIntervals = CreateNodeIntervals();
            nodeIntervals->Insert(interval, value);
            return;
        }

        TTreeNode* next = *nextPtr;
        if ( !next ) // create new node
            (*nextPtr) = next = InitNode(AllocNode(), key + nextKeyOffset);

        _ASSERT(next->m_Key == key + nextKeyOffset);
        node = next;
    }
}

bool CIntervalTree::DoDelete(TTreeNode* node, const interval_type& interval,
                             TTreeMapI value)
{
    _ASSERT(node);
    coordinate_type key = node->m_Key;
    if ( interval.GetFrom() > key ) {
        // left
        return DoDelete(node->m_Right, interval, value) &&
            !node->m_NodeIntervals && !node->m_Left;
    }
    else if ( interval.GetTo() < key ) {
        // right
        return DoDelete(node->m_Left, interval, value) &&
            !node->m_NodeIntervals && !node->m_Right;
    }
    else {
        // inside
        TTreeNodeInts* nodeIntervals = node->m_NodeIntervals;
        _ASSERT(nodeIntervals);

        if ( !nodeIntervals->Delete(interval, value) )
            return false; // node intervals non empty

        // remove node intervals
        DeleteNodeIntervals(nodeIntervals);
        node->m_NodeIntervals = 0;

        // delete node if it doesn't have leaves
        return !node->m_Left && !node->m_Right;
    }
}

void CIntervalTree::Destroy(void)
{
    ClearNode(&m_Root);
    m_ByX.clear();
}

CIntervalTree::iterator CIntervalTree::Insert(const interval_type& interval,
                                              const mapped_type& value)
{
    TTreeMapI iter = m_ByX.insert(TTreeMapValue(interval.GetFrom(),
                                                interval.GetTo(),
                                                value));
    DoInsert(interval, iter);

    return iterator(0, TTraits::GetMaxCoordinate(), &TTreeMap::get(iter));
}

CIntervalTree::const_iterator
CIntervalTree::IntervalsOverlapping(const interval_type& interval) const
{
    coordinate_type x = interval.GetFrom();
    coordinate_type y = interval.GetTo();

    const_iterator it(x, TTraits::GetMaxCoordinate(), 0, &m_Root);

    TTreeMapCI iter =
        m_ByX.lower_bound(TTreeMapValue(x + 1, 0, mapped_type()));
    if ( iter != m_ByX.end() && iter->GetKey() <= y ) {
        it.m_SearchLimit = y;
        it.m_CurrentMapValue = &*iter;
    }
    else {
        it.NextLevel();
    }
    return it;
}

CIntervalTree::iterator
CIntervalTree::IntervalsOverlapping(const interval_type& interval)
{
    coordinate_type x = interval.GetFrom();
    coordinate_type y = interval.GetTo();

    iterator it(x, TTraits::GetMaxCoordinate(), 0, &m_Root);

    TTreeMapI iter =
        m_ByX.lower_bound(TTreeMapValue(x + 1, 0, mapped_type()));
    if ( iter != m_ByX.end() && iter->GetKey() <= y ) {
        it.m_SearchLimit = y;
        it.m_CurrentMapValue = &TTreeMap::get(iter);
    }
    else {
        it.NextLevel();
    }
    return it;
}

CIntervalTree::TTreeNode* CIntervalTree::AllocNode(void)
{
    return m_NodeAllocator.allocate(1, (TTreeNode*) 0);
}

void CIntervalTree::DeallocNode(TTreeNode* node)
{
    m_NodeAllocator.deallocate(node, 1);
}

CIntervalTree::TTreeNodeInts* CIntervalTree::AllocNodeIntervals(void)
{
    return m_NodeIntervalsAllocator.allocate(1, (TTreeNodeInts*) 0);
}

void CIntervalTree::DeallocNodeIntervals(TTreeNodeInts* ptr)
{
    m_NodeIntervalsAllocator.deallocate(ptr, 1);
}

CIntervalTree::TTreeNodeInts* CIntervalTree::CreateNodeIntervals(void)
{
    TTreeNodeInts* ints = new (AllocNodeIntervals())TTreeNodeInts();
#if defined(_RWSTD_VER) && !defined(_RWSTD_STRICT_ANSI)
    ints->m_ByX.allocation_size(16);
    ints->m_ByY.allocation_size(16);
#endif
    return ints;
}

void CIntervalTree::DeleteNodeIntervals(TTreeNodeInts* ptr)
{
    if ( ptr ) {
        ptr->~TTreeNodeInts();
        DeallocNodeIntervals(ptr);
    }
}

void CIntervalTree::ClearNode(TTreeNode* node)
{
    DeleteNodeIntervals(node->m_NodeIntervals);

    DeleteNode(node->m_Left);
    DeleteNode(node->m_Right);
    node->m_Left = node->m_Right = 0;
}

pair<double, CIntervalTree::size_type> CIntervalTree::Stat(void) const
{
    SStat stat;
    stat.total = stat.count = stat.max = 0;
    Stat(&m_Root, stat);
    return make_pair(double(stat.total) / (double)stat.count, stat.max);
}

void CIntervalTree::Stat(const TTreeNode* node, SStat& stat) const
{
    if ( !node )
        return;

    if ( node->m_NodeIntervals ) {
        size_type len = node->m_NodeIntervals->m_ByX.size();
        ++stat.count;
        stat.total += len;
        stat.max = max(stat.max, len);
    }

    Stat(node->m_Right, stat);
    Stat(node->m_Left, stat);
}

END_NCBI_SCOPE
