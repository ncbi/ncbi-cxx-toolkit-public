#if defined(CONTINFO__HPP)  &&  !defined(CONTINFO__INL)
#define CONTINFO__INL

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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/09/18 20:00:00  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/

inline
TTypeInfo CContainerTypeInfo::GetElementType(void) const
{
    return m_ElementType.Get();
}

inline
bool CContainerTypeInfo::RandomElementsOrder(void) const
{
    return m_RandomOrder;
}

inline
CContainerElementIterator::TIterator* CContainerElementIterator::CloneIterator(void) const
{
    TIterator* i = m_Iterator.get();
    return i? i->Clone(): 0;
}

inline
CContainerElementIterator::CContainerElementIterator(void)
    : m_ElementType(0), m_Valid(false)
{
}

inline
CContainerElementIterator::CContainerElementIterator(TObjectPtr containerPtr,
                                                     const CContainerTypeInfo* containerType)
    : m_ElementType(containerType->GetElementType()),
      m_Iterator(containerType->NewIterator()),
      m_Valid(m_Iterator->Init(containerPtr))
{
}

inline
CContainerElementIterator::CContainerElementIterator(const CContainerElementIterator& src)
    : m_ElementType(src.m_ElementType),
      m_Iterator(src.CloneIterator()),
      m_Valid(src.m_Valid)
{
}

inline
CContainerElementIterator& CContainerElementIterator::operator=(const CContainerElementIterator& src)
{
    m_Valid = false;
    m_ElementType = src.m_ElementType;
    m_Iterator = src.CloneIterator();
    m_Valid = src.m_Valid;
    return *this;
}

inline
void CContainerElementIterator::Init(TObjectPtr containerPtr,
                                     const CContainerTypeInfo* containerType)
{
    m_Valid = false;
    m_ElementType = containerType->GetElementType();
    m_Iterator.reset(containerType->NewIterator());
    m_Valid = m_Iterator->Init(containerPtr);
}

inline
TTypeInfo CContainerElementIterator::GetElementType(void) const
{
    return m_ElementType;
}

inline
bool CContainerElementIterator::Valid(void) const
{
    return m_Valid;
}

inline
void CContainerElementIterator::Next(void)
{
    _ASSERT(m_Valid);
    m_Valid = m_Iterator->Next();
}

inline
void CContainerElementIterator::Erase(void)
{
    _ASSERT(m_Valid);
    m_Valid = m_Iterator->Erase();
}

inline
pair<TObjectPtr, TTypeInfo> CContainerElementIterator::Get(void) const
{
    _ASSERT(m_Valid);
    return make_pair(m_Iterator->GetElementPtr(), GetElementType());
}

inline
CConstContainerElementIterator::TIterator* CConstContainerElementIterator::CloneIterator(void) const
{
    TIterator* i = m_Iterator.get();
    return i? i->Clone(): 0;
}

inline
CConstContainerElementIterator::CConstContainerElementIterator(void)
    : m_ElementType(0), m_Valid(false)
{
}

inline
CConstContainerElementIterator::CConstContainerElementIterator(TConstObjectPtr containerPtr,
                                                               const CContainerTypeInfo* containerType)
    : m_ElementType(containerType->GetElementType()),
      m_Iterator(containerType->NewConstIterator()),
      m_Valid(m_Iterator->Init(containerPtr))
{
}

inline
CConstContainerElementIterator::CConstContainerElementIterator(const CConstContainerElementIterator& src)
    : m_ElementType(src.m_ElementType),
      m_Iterator(src.CloneIterator()),
      m_Valid(src.m_Valid)
{
}

inline
CConstContainerElementIterator&
CConstContainerElementIterator::operator=(const CConstContainerElementIterator& src)
{
    m_Valid = false;
    m_ElementType = src.m_ElementType;
    m_Iterator.reset(src.CloneIterator());
    m_Valid = src.m_Valid;
    return *this;
}

inline
void CConstContainerElementIterator::Init(TConstObjectPtr containerPtr,
                                          const CContainerTypeInfo* containerType)
{
    m_Valid = false;
    m_ElementType = containerType->GetElementType();
    m_Iterator.reset(containerType->NewConstIterator());
    m_Valid = m_Iterator->Init(containerPtr);
}

inline
TTypeInfo CConstContainerElementIterator::GetElementType(void) const
{
    return m_ElementType;
}

inline
bool CConstContainerElementIterator::Valid(void) const
{
    return m_Valid;
}

inline
void CConstContainerElementIterator::Next(void)
{
    _ASSERT(m_Valid);
    m_Valid = m_Iterator->Next();
}

inline
pair<TConstObjectPtr, TTypeInfo> CConstContainerElementIterator::Get(void) const
{
    _ASSERT(m_Valid);
    return make_pair(m_Iterator->GetElementPtr(), GetElementType());
}

#endif /* def CONTINFO__HPP  &&  ndef CONTINFO__INL */
