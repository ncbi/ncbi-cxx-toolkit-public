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



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2006/10/05 19:23:37  gouriano
* Moved from parent folder
*
* Revision 1.5  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.4  2000/10/03 17:22:33  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.3  2000/09/18 20:00:03  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.2  2000/06/16 16:31:05  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.1  2000/04/10 18:01:52  vasilche
* Added Erase() for STL types in type iterators.
*
* ===========================================================================
*/
