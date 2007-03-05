#if defined(MEMBERLIST__HPP)  &&  !defined(MEMBERLIST__INL)
#define MEMBERLIST__INL

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*/

inline
CItemInfo* CItemsInfo::x_GetItemInfo(TMemberIndex index) const
{
    _ASSERT(index >= FirstIndex() && index <= LastIndex());
    return m_Items[index - FirstIndex()].get();
}

inline
const CItemInfo* CItemsInfo::GetItemInfo(TMemberIndex index) const
{
    return x_GetItemInfo(index);
}

inline
CItemsInfo::CIterator::CIterator(const CItemsInfo& items)
    : m_CurrentIndex(items.FirstIndex()),
      m_LastIndex(items.LastIndex())
{
}

inline
CItemsInfo::CIterator::CIterator(const CItemsInfo& items, TMemberIndex index)
    : m_CurrentIndex(index),
      m_LastIndex(items.LastIndex())
{
    _ASSERT(index >= kFirstMemberIndex);
    _ASSERT(index <= (m_LastIndex + 1));
}

inline
void CItemsInfo::CIterator::SetIndex(TMemberIndex index)
{
    _ASSERT(index >= kFirstMemberIndex);
    _ASSERT(index <= (m_LastIndex + 1));
    m_CurrentIndex = index;
}

inline
CItemsInfo::CIterator& CItemsInfo::CIterator::operator=(TMemberIndex index)
{
    SetIndex(index);
    return *this;
}

inline
bool CItemsInfo::CIterator::Valid(void) const
{
    return m_CurrentIndex <= m_LastIndex;
}

inline
void CItemsInfo::CIterator::Next(void)
{
    ++m_CurrentIndex;
}

inline
void CItemsInfo::CIterator::operator++(void)
{
    Next();
}

inline
TMemberIndex CItemsInfo::CIterator::GetIndex(void) const
{
    return m_CurrentIndex;
}

inline
TMemberIndex CItemsInfo::CIterator::operator*(void) const
{
    return GetIndex();
}

inline
const CItemInfo* CItemsInfo::GetItemInfo(const CIterator& i) const
{
    return GetItemInfo(*i);
}

#endif /* def MEMBERLIST__HPP  &&  ndef MEMBERLIST__INL */
