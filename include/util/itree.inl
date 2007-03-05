#if defined(ITREE__HPP)  &&  !defined(ITREE__INL)
#define ITREE__INL

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
*   Inline methods of interval search tree.
*
* ===========================================================================
*/

inline
CIntervalTreeTraits::coordinate_type
CIntervalTreeTraits::GetMax(void)
{
    return interval_type::GetPositionMax();
}

inline
CIntervalTreeTraits::coordinate_type
CIntervalTreeTraits::GetMaxCoordinate(void)
{
    return GetMax() - 1;
}

inline
bool CIntervalTreeTraits::IsNormal(const interval_type& interval)
{
    return interval.GetFrom() >= 0 &&
        interval.GetFrom() <= interval.GetTo() &&
        interval.GetTo() <= GetMaxCoordinate();
}

template<typename Traits>
inline
CIntervalTreeIterator<Traits>::CIntervalTreeIterator(coordinate_type search_x,
                                                     coordinate_type searchLimit,
                                                     TMapValueP currentMapValue,
                                                     TTreeNodeP nextNode)
    : m_SearchX(search_x),
      m_SearchLimit(searchLimit),
      m_CurrentMapValue(currentMapValue),
      m_NextNode(nextNode)
{
}

template<typename Traits>
inline
CIntervalTreeIterator<Traits>::CIntervalTreeIterator(const TNCIterator& iter)
{
    TMap::Assign(*this, iter);
}

template<typename Traits>
inline
bool CIntervalTreeIterator<Traits>::InAuxMap(void) const
{
    return m_SearchLimit > m_SearchX;
}

template<typename Traits>
inline
typename CIntervalTreeIterator<Traits>::TTreeMapValueP
CIntervalTreeIterator<Traits>::GetTreeMapValue(void) const
{
    if ( InAuxMap() )
        return static_cast<TTreeMapValueP>(m_CurrentMapValue);
    else
        return &*static_cast<TNodeMapValueP>(m_CurrentMapValue)->m_Value;
}

template<typename Traits>
inline
void CIntervalTreeIterator<Traits>::Next(void)
{
    TMapValueP newMapValue = m_CurrentMapValue->GetNext();
    if ( newMapValue && newMapValue->GetKey() <= m_SearchLimit )
        m_CurrentMapValue = newMapValue;
    else
        NextLevel();
}

template<typename Traits>
inline
typename CIntervalTreeIterator<Traits>::interval_type
CIntervalTreeIterator<Traits>::GetInterval(void) const
{
    return GetTreeMapValue()->GetInterval();
}

template<typename Traits>
inline
CIntervalTreeIterator<Traits>& CIntervalTreeIterator<Traits>::operator++(void)
{
    Next();
    return *this;
}

template<typename Traits>
inline
typename CIntervalTreeIterator<Traits>::reference
CIntervalTreeIterator<Traits>::GetValue(void) const
{
    return GetTreeMapValue()->m_Value;
}

template<typename Traits>
void CIntervalTreeIterator<Traits>::NextLevel(void)
{
    // get iterator values
    coordinate_type x = m_SearchX;
    TTreeNodeP nextNode = m_NextNode;

    // 
    while ( nextNode ) {
        // get node values
        coordinate_type key = nextNode->m_Key;
        TTreeNodeIntsP nodeIntervals = nextNode->m_NodeIntervals;

        // new iterator values
        TMapValueP firstMapValue;
        coordinate_type searchLimit;

        if ( x < key ) {
            // by X
            if ( x == key )
                nextNode = 0;
            else
                nextNode = nextNode->m_Left;
            if ( !nodeIntervals )
                continue; // skip this node
            firstMapValue = nodeIntervals->m_ByX.GetStart();
            searchLimit = x;
        }
        else {
            // by Y
            nextNode = nextNode->m_Right;
            if ( !nodeIntervals )
                continue; // skip this node
            firstMapValue = nodeIntervals->m_ByY.GetStart();
            searchLimit = -x;
        }

        _ASSERT(firstMapValue);
        if ( firstMapValue->GetKey() <= searchLimit ) {
            m_CurrentMapValue = firstMapValue;
            m_SearchLimit = searchLimit;
            m_NextNode = nextNode;
            return; // found
        }
    }

    m_CurrentMapValue = 0;
}

template<class Traits>
inline
bool SIntervalTreeNodeIntervals<Traits>::Empty(void) const
{
    return m_ByX.empty();
}

template<class Traits>
void SIntervalTreeNodeIntervals<Traits>::Delete(TNodeMap& m,
                                                const TNodeMapValue& value)
{
    TNodeMapI it = m.lower_bound(value);
    _ASSERT(it != m.end());
    while ( it->m_Value != value.m_Value ) {
        ++it;
        _ASSERT(it != m.end());
        _ASSERT(it->GetKey() == value.GetKey());
    }
    m.erase(it);
}

template<class Traits>
inline
void SIntervalTreeNodeIntervals<Traits>::Insert(const interval_type& interval,
                                                TTreeMapI value)
{
    // insert interval
    m_ByX.insert(TNodeMapValue(interval.GetFrom(), value));
    m_ByY.insert(TNodeMapValue(-interval.GetTo(), value));
}

template<class Traits>
inline
bool SIntervalTreeNodeIntervals<Traits>::Delete(const interval_type& interval,
                                                TTreeMapI value)
{
    // erase interval
    Delete(m_ByX, TNodeMapValue(interval.GetFrom(), value));
    Delete(m_ByY, TNodeMapValue(-interval.GetTo(), value));
    return Empty();
}


inline
bool CIntervalTree::Empty(void) const
{
    return m_ByX.empty();
}

inline
CIntervalTree::const_iterator
CIntervalTree::End(void) const
{
    return const_iterator();
}

inline
CIntervalTree::const_iterator
CIntervalTree::AllIntervals(void) const
{
    if ( Empty() )
        return End();

    return const_iterator(0, TTraits::GetMaxCoordinate(), m_ByX.GetStart());
}

inline
CIntervalTree::const_iterator
CIntervalTree::IntervalsContaining(coordinate_type x) const
{
    const_iterator it(x, TTraits::GetMaxCoordinate(), 0, &m_Root);
    it.NextLevel();
    return it;
}

inline
CIntervalTree::iterator
CIntervalTree::End(void)
{
    return iterator();
}

inline
CIntervalTree::iterator
CIntervalTree::AllIntervals(void)
{
    if ( Empty() )
        return End();

    return iterator(0, TTraits::GetMaxCoordinate(), m_ByX.GetStart());
}

inline
CIntervalTree::iterator
CIntervalTree::IntervalsContaining(coordinate_type x)
{
    iterator it(x, TTraits::GetMaxCoordinate(), 0, &m_Root);
    it.NextLevel();
    return it;
}

inline
void CIntervalTree::Init(void)
{
}

inline
CIntervalTree::size_type CIntervalTree::Size(void) const
{
    return m_ByX.size();
}

inline
void CIntervalTree::DeleteNode(TTreeNode* node)
{
    if ( node ) {
        ClearNode(node);
        DeallocNode(node);
    }
}

inline
void CIntervalTree::Clear(void)
{
    Destroy();
    Init();
}

inline
CIntervalTree::TTreeNode* CIntervalTree::InitNode(TTreeNode* node,
                                                  coordinate_type key) const
{
    node->m_Key = key;
    node->m_Left = node->m_Right = 0;
    node->m_NodeIntervals = 0;
    return node;
}

inline
CIntervalTree::CIntervalTree(void)
{
    InitNode(&m_Root, 2); // should be some power of 2
    Init();
}

inline
CIntervalTree::~CIntervalTree(void)
{
    Destroy();
}

inline
void CIntervalTree::Assign(const_iterator& dst, const iterator& src)
{
    dst.m_SearchX = src.m_SearchX;
    dst.m_SearchLimit = src.m_SearchLimit;
    dst.m_CurrentMapValue = src.m_CurrentMapValue;
    dst.m_NextNode = src.m_NextNode;
}

inline
void CIntervalTree::Assign(iterator& dst, const iterator& src)
{
    dst.m_SearchX = src.m_SearchX;
    dst.m_SearchLimit = src.m_SearchLimit;
    dst.m_CurrentMapValue = src.m_CurrentMapValue;
    dst.m_NextNode = src.m_NextNode;
}

#endif /* def ITREE__HPP  &&  ndef ITREE__INL */
