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
* Revision 1.2  2000/10/13 16:28:30  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
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
CContainerTypeInfo::CConstIterator::CConstIterator(void)
    : m_ContainerType(0), m_IteratorData(0)
{
}

inline
CContainerTypeInfo::CConstIterator::~CConstIterator(void)
{
    const CContainerTypeInfo* containerType = m_ContainerType;
    if ( containerType )
        containerType->ReleaseIterator(*this);
}

inline
const CContainerTypeInfo* CContainerTypeInfo::CConstIterator::GetContainerType(void) const
{
    return m_ContainerType;
}

inline
void CContainerTypeInfo::CConstIterator::Reset(void)
{
    const CContainerTypeInfo* containerType = m_ContainerType;
    if ( containerType ) {
        containerType->ReleaseIterator(*this);
        m_ContainerType = 0;
        m_IteratorData = 0;
    }
}

inline
CContainerTypeInfo::CIterator::CIterator(void)
    : m_ContainerType(0), m_IteratorData(0)
{
}

inline
CContainerTypeInfo::CIterator::~CIterator(void)
{
    const CContainerTypeInfo* containerType = m_ContainerType;
    if ( containerType )
        containerType->ReleaseIterator(*this);
}

inline
const CContainerTypeInfo* CContainerTypeInfo::CIterator::GetContainerType(void) const
{
    return m_ContainerType;
}

inline
void CContainerTypeInfo::CIterator::Reset(void)
{
    const CContainerTypeInfo* containerType = m_ContainerType;
    if ( containerType ) {
        containerType->ReleaseIterator(*this);
        m_ContainerType = 0;
        m_IteratorData = 0;
    }
}

inline
bool CContainerTypeInfo::InitIterator(CConstIterator& it,
                                      TConstObjectPtr obj) const
{
    it.Reset();
    TNewIteratorResult data = m_InitIteratorConst(this, obj);
    it.m_ContainerType = this;
    it.m_IteratorData = data.first;
    return data.second;
}

inline
void CContainerTypeInfo::ReleaseIterator(CConstIterator& it) const
{
    _ASSERT(it.m_ContainerType == this);
    m_ReleaseIteratorConst(it.m_IteratorData);
    it.m_ContainerType = 0;
    it.m_IteratorData = 0;
}

inline
void CContainerTypeInfo::CopyIterator(CConstIterator& dst,
                                      const CConstIterator& src) const
{
    dst.Reset();
    _ASSERT(src.m_ContainerType == this);
    dst.m_IteratorData = m_CopyIteratorConst(src.m_IteratorData);
    dst.m_ContainerType = this;
}

inline
bool CContainerTypeInfo::NextElement(CConstIterator& it) const
{
    _ASSERT(it.m_ContainerType == this);
    TNewIteratorResult data = m_NextElementConst(it.m_IteratorData);
    it.m_IteratorData = data.first;
    return data.second;
}

inline
TConstObjectPtr CContainerTypeInfo::GetElementPtr(const CConstIterator& it) const
{
    _ASSERT(it.m_ContainerType == this);
    return m_GetElementPtrConst(it.m_IteratorData);
}

inline
bool CContainerTypeInfo::InitIterator(CIterator& it,
                                      TObjectPtr obj) const
{
    it.Reset();
    TNewIteratorResult data = m_InitIterator(this, obj);
    it.m_ContainerType = this;
    it.m_IteratorData = data.first;
    return data.second;
}

inline
void CContainerTypeInfo::ReleaseIterator(CIterator& it) const
{
    _ASSERT(it.m_ContainerType == this);
    m_ReleaseIterator(it.m_IteratorData);
    it.m_ContainerType = 0;
    it.m_IteratorData = 0;
}

inline
void CContainerTypeInfo::CopyIterator(CIterator& dst,
                                      const CIterator& src) const
{
    dst.Reset();
    _ASSERT(src.m_ContainerType == this);
    dst.m_IteratorData = m_CopyIterator(src.m_IteratorData);
    dst.m_ContainerType = this;
}

inline
bool CContainerTypeInfo::NextElement(CIterator& it) const
{
    _ASSERT(it.m_ContainerType == this);
    TNewIteratorResult data = m_NextElement(it.m_IteratorData);
    it.m_IteratorData = data.first;
    return data.second;
}

inline
TObjectPtr CContainerTypeInfo::GetElementPtr(const CIterator& it) const
{
    _ASSERT(it.m_ContainerType == this);
    return m_GetElementPtr(it.m_IteratorData);
}

inline
bool CContainerTypeInfo::EraseElement(CIterator& it) const
{
    _ASSERT(it.m_ContainerType == this);
    TNewIteratorResult data = m_EraseElement(it.m_IteratorData);
    it.m_IteratorData = data.first;
    return data.second;
}

inline
void CContainerTypeInfo::AddElement(TObjectPtr containerPtr,
                                    TConstObjectPtr elementPtr) const
{
    m_AddElement(this, containerPtr, elementPtr);
}

inline
void CContainerTypeInfo::AddElement(TObjectPtr containerPtr,
                                    CObjectIStream& in) const
{
    m_AddElementIn(this, containerPtr, in);
}

inline
CContainerElementIterator::CContainerElementIterator(void)
    : m_ElementType(0), m_Valid(false)
{
}

inline
CContainerElementIterator::CContainerElementIterator(TObjectPtr containerPtr,
                                                     const CContainerTypeInfo* containerType)
    : m_ElementType(containerType->GetElementType())
{
    m_Valid = containerType->InitIterator(m_Iterator, containerPtr);
}

inline
CContainerElementIterator::CContainerElementIterator(const CContainerElementIterator& src)
    : m_ElementType(src.m_ElementType),
      m_Valid(src.m_Valid)
{
    const CContainerTypeInfo* containerType =
        src.m_Iterator.GetContainerType();
    if ( containerType )
        containerType->CopyIterator(m_Iterator, src.m_Iterator);
}

inline
CContainerElementIterator& CContainerElementIterator::operator=(const CContainerElementIterator& src)
{
    m_Valid = false;
    m_Iterator.Reset();
    m_ElementType = src.m_ElementType;
    const CContainerTypeInfo* containerType =
        src.m_Iterator.GetContainerType();
    if ( containerType )
        containerType->CopyIterator(m_Iterator, src.m_Iterator);
    m_Valid = src.m_Valid;
    return *this;
}

inline
void CContainerElementIterator::Init(TObjectPtr containerPtr,
                                     const CContainerTypeInfo* containerType)
{
    m_Valid = false;
    m_Iterator.Reset();
    m_ElementType = containerType->GetElementType();
    m_Valid = containerType->InitIterator(m_Iterator, containerPtr);
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
    m_Valid = m_Iterator.GetContainerType()->NextElement(m_Iterator);
}

inline
void CContainerElementIterator::Erase(void)
{
    _ASSERT(m_Valid);
    m_Valid = m_Iterator.GetContainerType()->EraseElement(m_Iterator);
}

inline
pair<TObjectPtr, TTypeInfo> CContainerElementIterator::Get(void) const
{
    _ASSERT(m_Valid);
    return make_pair(m_Iterator.GetContainerType()->GetElementPtr(m_Iterator),
                     GetElementType());
}

inline
CConstContainerElementIterator::CConstContainerElementIterator(void)
    : m_ElementType(0), m_Valid(false)
{
}

inline
CConstContainerElementIterator::CConstContainerElementIterator(TConstObjectPtr containerPtr,
                                                               const CContainerTypeInfo* containerType)
    : m_ElementType(containerType->GetElementType())
{
    m_Valid = containerType->InitIterator(m_Iterator, containerPtr);
}

inline
CConstContainerElementIterator::CConstContainerElementIterator(const CConstContainerElementIterator& src)
    : m_ElementType(src.m_ElementType),
      m_Valid(src.m_Valid)
{
    const CContainerTypeInfo* containerType =
        src.m_Iterator.GetContainerType();
    if ( containerType )
        containerType->CopyIterator(m_Iterator, src.m_Iterator);
}

inline
CConstContainerElementIterator&
CConstContainerElementIterator::operator=(const CConstContainerElementIterator& src)
{
    m_Valid = false;
    m_Iterator.Reset();
    m_ElementType = src.m_ElementType;
    const CContainerTypeInfo* containerType =
        src.m_Iterator.GetContainerType();
    if ( containerType )
        containerType->CopyIterator(m_Iterator, src.m_Iterator);
    m_Valid = src.m_Valid;
    return *this;
}

inline
void CConstContainerElementIterator::Init(TConstObjectPtr containerPtr,
                                          const CContainerTypeInfo* containerType)
{
    m_Valid = false;
    m_Iterator.Reset();
    m_ElementType = containerType->GetElementType();
    m_Valid = containerType->InitIterator(m_Iterator, containerPtr);
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
    m_Valid = m_Iterator.GetContainerType()->NextElement(m_Iterator);
}

inline
pair<TConstObjectPtr, TTypeInfo> CConstContainerElementIterator::Get(void) const
{
    _ASSERT(m_Valid);
    return make_pair(m_Iterator.GetContainerType()->GetElementPtr(m_Iterator),
                     GetElementType());
}

#endif /* def CONTINFO__HPP  &&  ndef CONTINFO__INL */
