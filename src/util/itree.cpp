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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/01/11 15:08:20  vasilche
* Removed missing header.
*
* Revision 1.1  2001/01/11 15:00:44  vasilche
* Added CIntervalTree for seraching on set of intervals.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <util/itree.hpp>

BEGIN_NCBI_SCOPE

typedef CIntervalTreeTraits::coordinate_type coordinate_type;
typedef CIntervalTreeTraits::interval_type interval_type;

bool CIntervalTreeTraits::IsLimit(const TIntervalMap& intervalMap)
{
    _ASSERT(!intervalMap.empty());
    return IsLimit(intervalMap.begin()->first);
}

void CIntervalTreeTraits::AddLimit(TIntervalMap& intervalMap)
{
    _ASSERT(intervalMap.empty());
    intervalMap.insert(TIntervalMap::value_type(GetLimit(), 0));
    _ASSERT(IsLimit(intervalMap));
}

void CIntervalTreeIterator::NextLevel(void)
{
    _ASSERT(!ValidIter());
    coordinate_type x = m_SearchX;
    while ( m_NextNode ) {
        coordinate_type key = m_NextNode->m_Key;
        const SIntervalTreeNodeIntervals* nodeIntervals =
            m_NextNode->m_NodeIntervals;
        if ( x < key ) {
            // by X
            if ( x == key )
                m_NextNode = 0;
            else
                m_NextNode = m_NextNode->m_Left;
            if ( !nodeIntervals )
                continue; // skip this node
            m_Iter = nodeIntervals->m_ByX.begin();
            m_IterLimit = x;
        }
        else {
            // by Y
            m_NextNode = m_NextNode->m_Right;
            if ( !nodeIntervals )
                continue; // skip this node
            m_Iter = nodeIntervals->m_ByY.begin();
            m_IterLimit = -x;
        }
        if ( ValidIter() )
            return; // found
    }
    m_SearchX = -1; // mark finished iterator by negative X
}

SIntervalTreeNodeIntervals::SIntervalTreeNodeIntervals(const TIntervalMap& ref)
    : m_ByX(ref.key_comp(), ref.get_allocator()),
      m_ByY(ref.key_comp(), ref.get_allocator())
{
    traits::AddLimit(m_ByX);
    traits::AddLimit(m_ByY);
}

bool SIntervalTreeNodeIntervals::Empty(void) const
{
    return traits::IsLimit(m_ByX);
}

inline
void SIntervalTreeNodeIntervals::Insert(interval_type interval,
                                        const mapped_type& value)
{
    // check if this interval is new
    _ASSERT(m_ByX.count(interval) == 0);
    _ASSERT(m_ByY.count(traits::X2Y(interval)) == 0);

    // insert interval
    m_ByX[interval] = value;
    m_ByY[traits::X2Y(interval)] = value;
}

inline
bool SIntervalTreeNodeIntervals::Delete(interval_type interval)
{
    // check if this interval exists
    _ASSERT(m_ByX.count(interval) == 1);
    _ASSERT(m_ByY.count(traits::X2Y(interval)) == 1);

    // erase interval
    m_ByX.erase(interval);
    m_ByY.erase(traits::X2Y(interval));

    return Empty();
}

inline
void SIntervalTreeNodeIntervals::Replace(interval_type interval,
                                         const mapped_type& value)
{
    // check if this interval exists
    _ASSERT(m_ByX.count(interval) == 1);
    _ASSERT(m_ByY.count(traits::X2Y(interval)) == 1);

    // replace value
    m_ByX[interval] = value;
    m_ByY[traits::X2Y(interval)] = value;
}

inline
coordinate_type CIntervalTree::GetMaxRootCoordinate(void) const
{
    coordinate_type max = m_Root.m_Key * 2;
    if ( max <= 0 )
        max = traits::GetMaxCoordinate();
    return max;
}

inline
coordinate_type CIntervalTree::GetNextRootKey(void) const
{
    coordinate_type nextKey = m_Root.m_Key * 2;
    _ASSERT(nextKey > 0);
    return nextKey;
}

void CIntervalTree::DoInsert(interval_type interval, const mapped_type& value)
{
    _ASSERT(traits::IsNormal(interval));

    // ensure our tree covers specified interval
    if ( interval.GetTo() > GetMaxRootCoordinate() ) {
        // insert one more level on top
        if ( m_Root.m_Left || m_Root.m_Right || m_Root.m_NodeIntervals ) {
            // non empty tree, insert new root node
            do {
                SIntervalTreeNode* newLeft = AllocNode();
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

    SIntervalTreeNode* node = &m_Root;
    coordinate_type nodeSize = m_Root.m_Key;
    for ( ;; ) {
        coordinate_type key = node->m_Key;
        nodeSize = (nodeSize + 1) / 2;

        SIntervalTreeNode** nextPtr;
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
            SIntervalTreeNodeIntervals* nodeIntervals = node->m_NodeIntervals;
            if ( !nodeIntervals )
                node->m_NodeIntervals = nodeIntervals = CreateNodeIntervals();
            nodeIntervals->Insert(interval, value);
            return;
        }

        SIntervalTreeNode* next = *nextPtr;
        if ( !next ) // create new node
            (*nextPtr) = next = InitNode(AllocNode(), key + nextKeyOffset);

        _ASSERT(next->m_Key == key + nextKeyOffset);
        node = next;
    }
}

void CIntervalTree::DoReplace(SIntervalTreeNode* node,
                              interval_type interval, const mapped_type& value)
{
    for ( ;; ) {
        _ASSERT(node);
        coordinate_type key = node->m_Key;
        if ( interval.GetFrom() > key ) {
            node = node->m_Right;
        }
        else if ( interval.GetTo() < key ) {
            node = node->m_Left;
        }
        else {
            // found our tile
            _ASSERT(node->m_NodeIntervals);
            node->m_NodeIntervals->Replace(interval, value);
            return;
        }
    }
}

bool CIntervalTree::DoDelete(SIntervalTreeNode* node, interval_type interval)
{
    _ASSERT(node);
    coordinate_type key = node->m_Key;
    if ( interval.GetFrom() > key ) {
        // left
        return DoDelete(node->m_Right, interval) &&
            !node->m_NodeIntervals && !node->m_Left;
    }
    else if ( interval.GetTo() < key ) {
        // right
        return DoDelete(node->m_Left, interval) &&
            !node->m_NodeIntervals && !node->m_Right;
    }
    else {
        // inside
        SIntervalTreeNodeIntervals* nodeIntervals = node->m_NodeIntervals;
        _ASSERT(nodeIntervals);

        if ( !nodeIntervals->Delete(interval) )
            return false; // node intervals non empty

        // remove node intervals
        DeleteNodeIntervals(nodeIntervals);
        node->m_NodeIntervals = 0;

        // delete node if it doesn't have leaves
        return !node->m_Left && !node->m_Right;
    }
}

SIntervalTreeNode* CIntervalTree::AllocNode(void)
{
    return /*static_cast<SIntervalTreeNode*>*/(m_NodeAllocator.allocate(1));
}

void CIntervalTree::DeallocNode(SIntervalTreeNode* node)
{
    m_NodeAllocator.deallocate(node, 1);
}

SIntervalTreeNodeIntervals* CIntervalTree::AllocNodeIntervals(void)
{
    return /*static_cast<SIntervalTreeNodeIntervals*>*/(m_NodeIntervalsAllocator.allocate(1));
}

void CIntervalTree::DeallocNodeIntervals(SIntervalTreeNodeIntervals* ptr)
{
    m_NodeIntervalsAllocator.deallocate(ptr, 1);
}

SIntervalTreeNodeIntervals* CIntervalTree::CreateNodeIntervals(void)
{
    return new (AllocNodeIntervals())SIntervalTreeNodeIntervals(m_ByX);
}

void CIntervalTree::DeleteNodeIntervals(SIntervalTreeNodeIntervals* ptr)
{
    if ( ptr ) {
        ptr->~SIntervalTreeNodeIntervals();
        DeallocNodeIntervals(ptr);
    }
}

void CIntervalTree::ClearNode(SIntervalTreeNode* node)
{
    DeleteNodeIntervals(node->m_NodeIntervals);

    DeleteNode(node->m_Left);
    DeleteNode(node->m_Right);
    node->m_Left = node->m_Right = 0;
}

END_NCBI_SCOPE
