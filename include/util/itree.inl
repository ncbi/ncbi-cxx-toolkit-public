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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/01/11 15:00:38  vasilche
* Added CIntervalTree for seraching on set of intervals.
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
bool CIntervalTreeTraits::IsNormal(interval_type interval)
{
    return interval.GetFrom() >= 0 &&
        interval.GetFrom() <= interval.GetTo() &&
        interval.GetTo() <= GetMaxCoordinate();
}

inline
CIntervalTreeTraits::interval_type
CIntervalTreeTraits::GetLimit(void)
{
    coordinate_type max = GetMax();
    return interval_type(max, max);
}

inline
bool CIntervalTreeTraits::IsLimit(interval_type interval)
{
    coordinate_type max = GetMax();
    return interval.GetFrom() == max && interval.GetTo() == max;
}

inline
CIntervalTreeTraits::interval_type
CIntervalTreeTraits::X2Y(interval_type interval)
{
    return interval_type(-interval.GetTo(), interval.GetFrom());
}

inline
CIntervalTreeTraits::interval_type
CIntervalTreeTraits::Y2X(interval_type interval)
{
    return interval_type(interval.GetTo(), -interval.GetFrom());
}

inline
CIntervalTreeIterator::CIntervalTreeIterator(coordinate_type searchX,
                                             const SIntervalTreeNode* nextNode)
    : m_SearchX(searchX), m_NextNode(nextNode)
{
}

inline
bool CIntervalTreeIterator::Valid(void) const
{
    return m_SearchX >= 0;
}

inline
CIntervalTreeIterator::operator bool(void) const
{
    return Valid();
}

inline
bool CIntervalTreeIterator::operator!(void) const
{
    return !Valid();
}

inline
bool CIntervalTreeIterator::ValidIter(void) const
{
    return m_Iter->first.GetFrom() <= m_IterLimit;
}

inline
void CIntervalTreeIterator::Validate(void)
{
    if ( !ValidIter() )
        NextLevel();
}

inline
void CIntervalTreeIterator::SetIter(coordinate_type iterLimit,
                                    TIntervalMapCI iter)
{
    m_IterLimit = iterLimit;
    m_Iter = iter;
    Validate();
}

inline
void CIntervalTreeIterator::Next(void)
{
    ++m_Iter;
    Validate();
}

inline
CIntervalTreeIterator& CIntervalTreeIterator::operator++(void)
{
    Next();
    return *this;
}

inline
CIntervalTreeIterator::interval_type
CIntervalTreeIterator::GetInterval(void) const
{
    interval_type interval = m_Iter->first;
    if ( interval.GetFrom() < 0 ) // scan by Y using reversed interval
        return traits::Y2X(interval); // reverse it back
    else
        return interval;
}

inline
const CIntervalTreeIterator::mapped_type&
CIntervalTreeIterator::GetValue(void) const
{
    return m_Iter->second;
}

inline
void CIntervalTree::Add(interval_type interval, const mapped_type& value)
{
    _ASSERT(!traits::IsLimit(interval));
    pair<TIntervalMapI, bool> ins =
        m_ByX.insert(TIntervalMap::value_type(interval, value));
    if ( !ins.second ) {
        THROW1_TRACE(runtime_error,
                     "CIntervalTree::Add: interval already exists");
    }
    else {
        // insert new value
        DoInsert(interval, value);
    }
}

inline
void CIntervalTree::Replace(interval_type interval, const mapped_type& value)
{
    _ASSERT(!traits::IsLimit(interval));
    pair<TIntervalMapI, bool> ins =
        m_ByX.insert(TIntervalMap::value_type(interval, value));
    if ( !ins.second ) {
        // replace old value
        ins.first->second = value;
        DoReplace(&m_Root, interval, value);
    }
    else {
        THROW1_TRACE(runtime_error,
                     "CIntervalTree::Replace: interval doesn't exist");
    }
}

inline
bool CIntervalTree::Add(interval_type interval,
                        const mapped_type& value,
                        const nothrow_t&)
{
    _ASSERT(!traits::IsLimit(interval));
    pair<TIntervalMapI, bool> ins =
        m_ByX.insert(TIntervalMap::value_type(interval, value));
    if ( !ins.second ) {
        return false;
    }
    else {
        // insert new value
        DoInsert(interval, value);
        return true;
    }
}

inline
bool CIntervalTree::Replace(interval_type interval,
                            const mapped_type& value,
                            const nothrow_t&)
{
    _ASSERT(!traits::IsLimit(interval));
    pair<TIntervalMapI, bool> ins =
        m_ByX.insert(TIntervalMap::value_type(interval, value));
    if ( !ins.second ) {
        // replace old value
        ins.first->second = value;
        DoReplace(&m_Root, interval, value);
        return true;
    }
    else {
        return false;
    }
}

inline
bool CIntervalTree::Set(interval_type interval, const mapped_type& value)
{
    _ASSERT(!traits::IsLimit(interval));
    pair<TIntervalMapI, bool> ins =
        m_ByX.insert(TIntervalMap::value_type(interval, value));
    if ( !ins.second ) {
        // replace old value
        ins.first->second = value;
        DoReplace(&m_Root, interval, value);
        return false;
    }
    else {
        // insert new value
        DoInsert(interval, value);
        return true;
    }
}

inline
void CIntervalTree::Delete(interval_type interval)
{
    _ASSERT(!traits::IsLimit(interval));
    if ( !m_ByX.erase(interval) ) {
        THROW1_TRACE(runtime_error,
                     "CIntervalTree::Delete: interval desn't exist");
    }
    else {
        DoDelete(&m_Root, interval);
    }
}

inline
bool CIntervalTree::Delete(interval_type interval,
                           const nothrow_t&)
{
    _ASSERT(!traits::IsLimit(interval));
    if ( !m_ByX.erase(interval) ) {
        return false;
    }
    else {
        DoDelete(&m_Root, interval);
        return true;
    }
}

inline
bool CIntervalTree::Reset(interval_type interval)
{
    return Delete(interval, nothrow);
}

inline
CIntervalTree::const_iterator
CIntervalTree::AllIntervals(void) const
{
    const_iterator it(0, 0);
    it.SetIter(traits::GetMax() - 1, m_ByX.begin());
    return it;
}

inline
CIntervalTree::const_iterator
CIntervalTree::IntervalsContaining(coordinate_type x) const
{
    const_iterator it(x, &m_Root);
    it.NextLevel();
    return it;
}

inline
CIntervalTree::const_iterator
CIntervalTree::IntervalsOverlapping(interval_type interval) const
{
    coordinate_type x = interval.GetFrom();
    coordinate_type y = interval.GetTo();

    const_iterator it(x, &m_Root);
    it.SetIter(y, m_ByX.lower_bound(interval_type(x + 1, 0)));
    return it;
}

inline
bool CIntervalTree::Empty(void) const
{
    return traits::IsLimit(m_ByX);
}

inline
void CIntervalTree::Init(void)
{
    _ASSERT(m_ByX.empty());
    traits::AddLimit(m_ByX);
    _ASSERT(Empty());
}

inline
void CIntervalTree::Destroy(void)
{
    ClearNode(&m_Root);
    m_ByX.clear();
}

inline
CIntervalTree::size_type CIntervalTree::Size(void) const
{
    return m_ByX.size() - 1;
}

inline
void CIntervalTree::DeleteNode(SIntervalTreeNode* node)
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
SIntervalTreeNode* CIntervalTree::InitNode(SIntervalTreeNode* node,
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

#endif /* def ITREE__HPP  &&  ndef ITREE__INL */
