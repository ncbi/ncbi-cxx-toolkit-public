#if defined(OBJECTITER__HPP)  &&  !defined(OBJECTITER__INL)
#define OBJECTITER__INL

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

/////////////////////////////////////////////////////////////////////////////
// iterators
/////////////////////////////////////////////////////////////////////////////

// container interface

inline
CConstObjectInfoEI::CConstObjectInfoEI(void)
{
}

inline
bool CConstObjectInfoEI::CheckValid(void) const
{
    return m_Iterator.Valid();
}

inline
bool CConstObjectInfoEI::Valid(void) const
{
    return CheckValid();
}

inline
void CConstObjectInfoEI::Next(void)
{
    _ASSERT(CheckValid());
    m_Iterator.Next();
}

inline
CConstObjectInfoEI& CConstObjectInfoEI::operator++(void)
{
    Next();
    return *this;
}

inline
CConstObjectInfo CConstObjectInfoEI::GetElement(void) const
{
    _ASSERT(CheckValid());
    return m_Iterator.Get();
}

inline
CConstObjectInfo CConstObjectInfoEI::operator*(void) const
{
    _ASSERT(CheckValid());
    return m_Iterator.Get();
}

/*
inline
CConstObjectInfo CConstObjectInfoEI::operator->(void) const
{
    _ASSERT(CheckValid());
    return m_Iterator.Get();
}
*/

inline
CObjectInfoEI::CObjectInfoEI(void)
{
}

inline
bool CObjectInfoEI::CheckValid(void) const
{
    return m_Iterator.Valid();
}

inline
bool CObjectInfoEI::Valid(void) const
{
    return CheckValid();
}

inline
void CObjectInfoEI::Next(void)
{
    _ASSERT(CheckValid());
    m_Iterator.Next();
}

inline
CObjectInfoEI& CObjectInfoEI::operator++(void)
{
    Next();
    return *this;
}

inline
CObjectInfo CObjectInfoEI::GetElement(void) const
{
    _ASSERT(CheckValid());
    return m_Iterator.Get();
}

inline
CObjectInfo CObjectInfoEI::operator*(void) const
{
    _ASSERT(CheckValid());
    return m_Iterator.Get();
}

inline
void CObjectInfoEI::Erase(void)
{
    _ASSERT(CheckValid());
    m_Iterator.Erase();
}

// class interface

inline
CObjectTypeInfoII::CObjectTypeInfoII(void)
    : m_ItemIndex(kFirstMemberIndex),
      m_LastItemIndex(kInvalidMember)
{
}

inline
void CObjectTypeInfoII::Init(const CClassTypeInfoBase* typeInfo,
                             TMemberIndex index)
{
    m_OwnerType = typeInfo;
    m_ItemIndex = index;
    m_LastItemIndex = typeInfo->GetItems().LastIndex();
}

inline
void CObjectTypeInfoII::Init(const CClassTypeInfoBase* typeInfo)
{
    Init(typeInfo, kFirstMemberIndex);
}

inline
CObjectTypeInfoII::CObjectTypeInfoII(const CClassTypeInfoBase* typeInfo)
{
    Init(typeInfo);
}

inline
CObjectTypeInfoII::CObjectTypeInfoII(const CClassTypeInfoBase* typeInfo,
                                     TMemberIndex index)
{
    Init(typeInfo, index);
}

inline
const CObjectTypeInfo& CObjectTypeInfoII::GetOwnerType(void) const
{
    return m_OwnerType;
}

inline
const CClassTypeInfoBase* CObjectTypeInfoII::GetClassTypeInfoBase(void) const
{
    return CTypeConverter<CClassTypeInfoBase>::
        SafeCast(GetOwnerType().GetTypeInfo());
}

inline
bool CObjectTypeInfoII::CheckValid(void) const
{
    return m_ItemIndex >= kFirstMemberIndex &&
        m_ItemIndex <= m_LastItemIndex;
}

inline
TMemberIndex CObjectTypeInfoII::GetItemIndex(void) const
{
    _ASSERT(CheckValid());
    return m_ItemIndex;
}

inline
const CItemInfo* CObjectTypeInfoII::GetItemInfo(void) const
{
    return GetClassTypeInfoBase()->GetItems().GetItemInfo(GetItemIndex());
}

inline
const string& CObjectTypeInfoII::GetAlias(void) const
{
    return GetItemInfo()->GetId().GetName();
}

inline
bool CObjectTypeInfoII::Valid(void) const
{
    return CheckValid();
}

inline
void CObjectTypeInfoII::Next(void)
{
    _ASSERT(CheckValid());
    ++m_ItemIndex;
}

inline
bool CObjectTypeInfoII::operator==(const CObjectTypeInfoII& iter) const
{
    return GetOwnerType() == iter.GetOwnerType() &&
        GetItemIndex() == iter.GetItemIndex();
}

inline
bool CObjectTypeInfoII::operator!=(const CObjectTypeInfoII& iter) const
{
    return GetOwnerType() != iter.GetOwnerType() ||
        GetItemIndex() == iter.GetItemIndex();
}

// CObjectTypeInfoMI //////////////////////////////////////////////////////

inline
CObjectTypeInfoMI::CObjectTypeInfoMI(void)
{
}

inline
CObjectTypeInfoMI::CObjectTypeInfoMI(const CObjectTypeInfo& info)
    : CParent(info.GetClassTypeInfo())
{
}

inline
CObjectTypeInfoMI::CObjectTypeInfoMI(const CObjectTypeInfo& info,
                                     TMemberIndex index)
    : CParent(info.GetClassTypeInfo(), index)
{
}

inline
CObjectTypeInfoMI& CObjectTypeInfoMI::operator++(void)
{
    Next();
    return *this;
}

inline
void CObjectTypeInfoMI::Init(const CObjectTypeInfo& info)
{
    CParent::Init(info.GetClassTypeInfo());
}

inline
void CObjectTypeInfoMI::Init(const CObjectTypeInfo& info,
                             TMemberIndex index)
{
    CParent::Init(info.GetClassTypeInfo(), index);
}

inline
CObjectTypeInfoMI& CObjectTypeInfoMI::operator=(const CObjectTypeInfo& info)
{
    Init(info);
    return *this;
}

inline
const CClassTypeInfo* CObjectTypeInfoMI::GetClassTypeInfo(void) const
{
    return GetOwnerType().GetClassTypeInfo();
}

inline
CObjectTypeInfo CObjectTypeInfoMI::GetClassType(void) const
{
    return GetOwnerType();
}

inline
TMemberIndex CObjectTypeInfoMI::GetMemberIndex(void) const
{
    return GetItemIndex();
}

inline
const CMemberInfo* CObjectTypeInfoMI::GetMemberInfo(void) const
{
    return GetClassTypeInfo()->GetMemberInfo(GetMemberIndex());
}

inline
CMemberInfo* CObjectTypeInfoMI::GetNCMemberInfo(void) const
{
    return const_cast<CMemberInfo*>(GetMemberInfo());
}

inline
CObjectTypeInfoMI::operator CObjectTypeInfo(void) const
{
    return GetMemberInfo()->GetTypeInfo();
}

inline
CObjectTypeInfo CObjectTypeInfoMI::GetMemberType(void) const
{
    return GetMemberInfo()->GetTypeInfo();
}

inline
CObjectTypeInfo CObjectTypeInfoMI::operator*(void) const
{
    return GetMemberInfo()->GetTypeInfo();
}

// CObjectTypeInfoVI //////////////////////////////////////////////////////

inline
CObjectTypeInfoVI::CObjectTypeInfoVI(const CObjectTypeInfo& info)
    : CParent(info.GetChoiceTypeInfo())
{
}

inline
CObjectTypeInfoVI::CObjectTypeInfoVI(const CObjectTypeInfo& info,
                                     TMemberIndex index)
    : CParent(info.GetChoiceTypeInfo(), index)
{
}

inline
CObjectTypeInfoVI& CObjectTypeInfoVI::operator++(void)
{
    Next();
    return *this;
}

inline
void CObjectTypeInfoVI::Init(const CObjectTypeInfo& info)
{
    CParent::Init(info.GetChoiceTypeInfo());
}

inline
void CObjectTypeInfoVI::Init(const CObjectTypeInfo& info,
                             TMemberIndex index)
{
    CParent::Init(info.GetChoiceTypeInfo(), index);
}

inline
CObjectTypeInfoVI& CObjectTypeInfoVI::operator=(const CObjectTypeInfo& info)
{
    Init(info);
    return *this;
}

inline
const CChoiceTypeInfo* CObjectTypeInfoVI::GetChoiceTypeInfo(void) const
{
    return GetOwnerType().GetChoiceTypeInfo();
}

inline
CObjectTypeInfo CObjectTypeInfoVI::GetChoiceType(void) const
{
    return GetOwnerType();
}

inline
TMemberIndex CObjectTypeInfoVI::GetVariantIndex(void) const
{
    return GetItemIndex();
}

inline
const CVariantInfo* CObjectTypeInfoVI::GetVariantInfo(void) const
{
    return GetChoiceTypeInfo()->GetVariantInfo(GetVariantIndex());
}

inline
CVariantInfo* CObjectTypeInfoVI::GetNCVariantInfo(void) const
{
    return const_cast<CVariantInfo*>(GetVariantInfo());
}

inline
CObjectTypeInfo CObjectTypeInfoVI::GetVariantType(void) const
{
    return GetVariantInfo()->GetTypeInfo();
}

inline
CObjectTypeInfo CObjectTypeInfoVI::operator*(void) const
{
    return GetVariantInfo()->GetTypeInfo();
}

// CConstObjectInfoMI //////////////////////////////////////////////////////

inline
CConstObjectInfoMI::CConstObjectInfoMI(void)
{
}

inline
CConstObjectInfoMI::CConstObjectInfoMI(const CConstObjectInfo& object)
    : CParent(object), m_Object(object)
{
    _ASSERT(object);
}

inline
CConstObjectInfoMI::CConstObjectInfoMI(const CConstObjectInfo& object,
                                       TMemberIndex index)
    : CParent(object, index), m_Object(object)
{
    _ASSERT(object);
}

inline
const CConstObjectInfo&
CConstObjectInfoMI::GetClassObject(void) const
{
    return m_Object;
}

inline
CConstObjectInfoMI&
CConstObjectInfoMI::operator=(const CConstObjectInfo& object)
{
    _ASSERT(object);
    CParent::Init(object);
    m_Object = object;
    return *this;
}

inline
bool CConstObjectInfoMI::IsSet(void) const
{
    return CParent::IsSet(GetClassObject());
}

inline
CConstObjectInfo CConstObjectInfoMI::GetMember(void) const
{
    return GetMemberPair();
}

inline
CConstObjectInfo CConstObjectInfoMI::operator*(void) const
{
    return GetMemberPair();
}

inline
CObjectInfoMI::CObjectInfoMI(void)
{
}

inline
CObjectInfoMI::CObjectInfoMI(const CObjectInfo& object)
    : CParent(object), m_Object(object)
{
    _ASSERT(object);
}

inline
CObjectInfoMI::CObjectInfoMI(const CObjectInfo& object,
                             TMemberIndex index)
    : CParent(object, index), m_Object(object)
{
    _ASSERT(object);
}

inline
CObjectInfoMI& CObjectInfoMI::operator=(const CObjectInfo& object)
{
    _ASSERT(object);
    CParent::Init(object);
    m_Object = object;
    return *this;
}

inline
const CObjectInfo& CObjectInfoMI::GetClassObject(void) const
{
    return m_Object;
}

inline
bool CObjectInfoMI::IsSet(void) const
{
    return CParent::IsSet(GetClassObject());
}

inline
void CObjectInfoMI::Reset(void)
{
    Erase();
}

inline
CObjectInfo CObjectInfoMI::GetMember(void) const
{
    return GetMemberPair();
}

inline
CObjectInfo CObjectInfoMI::operator*(void) const
{
    return GetMemberPair();
}

// choice interface

inline
CObjectTypeInfoCV::CObjectTypeInfoCV(void)
    : m_ChoiceTypeInfo(0), m_VariantIndex(kEmptyChoice)
{
}

inline
CObjectTypeInfoCV::CObjectTypeInfoCV(const CObjectTypeInfo& info)
    : m_ChoiceTypeInfo(info.GetChoiceTypeInfo()), m_VariantIndex(kEmptyChoice)
{
}

inline
CObjectTypeInfoCV::CObjectTypeInfoCV(const CConstObjectInfo& object)
{
    const CChoiceTypeInfo* choiceInfo =
        m_ChoiceTypeInfo = object.GetChoiceTypeInfo();
    m_VariantIndex = choiceInfo->GetIndex(object.GetObjectPtr());
    _ASSERT(m_VariantIndex <= choiceInfo->GetVariants().LastIndex());
}

inline
CObjectTypeInfoCV::CObjectTypeInfoCV(const CObjectTypeInfo& info,
                                     TMemberIndex index)
{
    const CChoiceTypeInfo* choiceInfo =
        m_ChoiceTypeInfo = info.GetChoiceTypeInfo();
    if ( index > choiceInfo->GetVariants().LastIndex() )
        index = kEmptyChoice;
    m_VariantIndex = index;
}

inline
const CChoiceTypeInfo* CObjectTypeInfoCV::GetChoiceTypeInfo(void) const
{
    return m_ChoiceTypeInfo;
}

inline
TMemberIndex CObjectTypeInfoCV::GetVariantIndex(void) const
{
    return m_VariantIndex;
}

inline
bool CObjectTypeInfoCV::Valid(void) const
{
    return GetVariantIndex() != kEmptyChoice;
}

inline
void CObjectTypeInfoCV::Init(const CObjectTypeInfo& info)
{
    m_ChoiceTypeInfo = info.GetChoiceTypeInfo();
    m_VariantIndex = kEmptyChoice;
}

inline
void CObjectTypeInfoCV::Init(const CObjectTypeInfo& info,
                             TMemberIndex index)
{
    m_ChoiceTypeInfo = info.GetChoiceTypeInfo();
    m_VariantIndex = index;
}

inline
CObjectTypeInfoCV& CObjectTypeInfoCV::operator=(const CObjectTypeInfo& info)
{
    m_ChoiceTypeInfo = info.GetChoiceTypeInfo();
    m_VariantIndex = kEmptyChoice;
    return *this;
}

inline
bool CObjectTypeInfoCV::operator==(const CObjectTypeInfoCV& iter) const
{
    _ASSERT(GetChoiceTypeInfo() == iter.GetChoiceTypeInfo());
    return GetVariantIndex() == iter.GetVariantIndex();
}

inline
bool CObjectTypeInfoCV::operator!=(const CObjectTypeInfoCV& iter) const
{
    _ASSERT(GetChoiceTypeInfo() == iter.GetChoiceTypeInfo());
    return GetVariantIndex() != iter.GetVariantIndex();
}

inline
const CVariantInfo* CObjectTypeInfoCV::GetVariantInfo(void) const
{
    return GetChoiceTypeInfo()->GetVariantInfo(GetVariantIndex());
}

inline
CVariantInfo* CObjectTypeInfoCV::GetNCVariantInfo(void) const
{
    return const_cast<CVariantInfo*>(GetVariantInfo());
}

inline
const string& CObjectTypeInfoCV::GetAlias(void) const
{
    return GetVariantInfo()->GetId().GetName();
}

inline
CObjectTypeInfo CObjectTypeInfoCV::GetChoiceType(void) const
{
    return GetChoiceTypeInfo();
}

inline
CObjectTypeInfo CObjectTypeInfoCV::GetVariantType(void) const
{
    return GetVariantInfo()->GetTypeInfo();
}

inline
CObjectTypeInfo CObjectTypeInfoCV::operator*(void) const
{
    return GetVariantInfo()->GetTypeInfo();
}

inline
CConstObjectInfoCV::CConstObjectInfoCV(void)
{
}

inline
CConstObjectInfoCV::CConstObjectInfoCV(const CConstObjectInfo& object)
    : CParent(object), m_Object(object)
{
}

inline
CConstObjectInfoCV::CConstObjectInfoCV(const CConstObjectInfo& object,
                                       TMemberIndex index)
    : CParent(object, index), m_Object(object)
{
}

inline
const CConstObjectInfo& CConstObjectInfoCV::GetChoiceObject(void) const
{
    return m_Object;
}

inline
CConstObjectInfoCV& CConstObjectInfoCV::operator=(const CConstObjectInfo& object)
{
    CParent::Init(object);
    m_Object = object;
    return *this;
}

inline
CConstObjectInfo CConstObjectInfoCV::GetVariant(void) const
{
    return GetVariantPair();
}

inline
CConstObjectInfo CConstObjectInfoCV::operator*(void) const
{
    return GetVariantPair();
}

inline
CObjectInfoCV::CObjectInfoCV(void)
{
}

inline
CObjectInfoCV::CObjectInfoCV(const CObjectInfo& object)
    : CParent(object), m_Object(object)
{
}

inline
CObjectInfoCV::CObjectInfoCV(const CObjectInfo& object,
                             TMemberIndex index)
    : CParent(object, index), m_Object(object)
{
}

inline
const CObjectInfo& CObjectInfoCV::GetChoiceObject(void) const
{
    return m_Object;
}

inline
CObjectInfoCV& CObjectInfoCV::operator=(const CObjectInfo& object)
{
    CParent::Init(object);
    m_Object = object;
    return *this;
}

inline
CObjectInfo CObjectInfoCV::GetVariant(void) const
{
    return GetVariantPair();
}

inline
CObjectInfo CObjectInfoCV::operator*(void) const
{
    return GetVariantPair();
}

/////////////////////////////////////////////////////////////////////////////
// iterator getters
/////////////////////////////////////////////////////////////////////////////

// container interface

inline
CConstObjectInfoEI CConstObjectInfo::BeginElements(void) const
{
    return CElementIterator(*this);
}

inline
CObjectInfoEI CObjectInfo::BeginElements(void) const
{
    return CElementIterator(*this);
}

// class interface

inline
CObjectTypeInfoMI CObjectTypeInfo::BeginMembers(void) const
{
    return CMemberIterator(*this);
}

inline
CObjectTypeInfoMI CObjectTypeInfo::GetMemberIterator(TMemberIndex index) const
{
    return CMemberIterator(*this, index);
}

inline
CObjectTypeInfoMI CObjectTypeInfo::FindMember(const string& name) const
{
    return GetMemberIterator(FindMemberIndex(name));
}

inline
CObjectTypeInfoMI CObjectTypeInfo::FindMemberByTag(int tag) const
{
    return GetMemberIterator(FindMemberIndex(tag));
}

inline
CObjectTypeInfoVI CObjectTypeInfo::BeginVariants(void) const
{
    return CVariantIterator(*this);
}

inline
CObjectTypeInfoVI CObjectTypeInfo::GetVariantIterator(TMemberIndex index) const
{
    return CVariantIterator(*this, index);
}

inline
CObjectTypeInfoVI CObjectTypeInfo::FindVariant(const string& name) const
{
    return GetVariantIterator(FindVariantIndex(name));
}

inline
CObjectTypeInfoVI CObjectTypeInfo::FindVariantByTag(int tag) const
{
    return GetVariantIterator(FindVariantIndex(tag));
}

inline
CConstObjectInfoMI CConstObjectInfo::GetMember(CObjectTypeInfoMI member) const
{
    return CMemberIterator(*this, member.GetMemberIndex());
}


inline
CConstObjectInfoMI CConstObjectInfo::BeginMembers(void) const
{
    return CMemberIterator(*this);
}

inline
CConstObjectInfoMI CConstObjectInfo::GetClassMemberIterator(TMemberIndex index) const
{
    return CMemberIterator(*this, index);
}

inline
CConstObjectInfoMI CConstObjectInfo::FindClassMember(const string& name) const
{
    return GetClassMemberIterator(FindMemberIndex(name));
}

inline
CConstObjectInfoMI CConstObjectInfo::FindClassMemberByTag(int tag) const
{
    return GetClassMemberIterator(FindMemberIndex(tag));
}

inline
CObjectInfoMI CObjectInfo::GetMember(CObjectTypeInfoMI member) const
{
    return CMemberIterator(*this, member.GetMemberIndex());
}

inline
CObjectInfoMI CObjectInfo::BeginMembers(void) const
{
    return CMemberIterator(*this);
}

inline
CObjectInfoMI CObjectInfo::GetClassMemberIterator(TMemberIndex index) const
{
    return CMemberIterator(*this, index);
}

inline
CObjectInfoMI CObjectInfo::FindClassMember(const string& name) const
{
    return GetClassMemberIterator(FindMemberIndex(name));
}

inline
CObjectInfoMI CObjectInfo::FindClassMemberByTag(int tag) const
{
    return GetClassMemberIterator(FindMemberIndex(tag));
}

// choice interface

inline
CConstObjectInfoCV CConstObjectInfo::GetCurrentChoiceVariant(void) const
{
    return CChoiceVariant(*this, GetCurrentChoiceVariantIndex());
}

inline
CObjectInfoCV CObjectInfo::GetCurrentChoiceVariant(void) const
{
    return CChoiceVariant(*this, GetCurrentChoiceVariantIndex());
}

#endif /* def OBJECTITER__HPP  &&  ndef OBJECTITER__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2005/03/17 21:07:42  vasilche
* Removed enforced check for call of IsValid().
*
* Revision 1.7  2005/01/24 17:05:48  vasilche
* Safe boolean operators.
*
* Revision 1.6  2003/07/17 22:49:31  vasilche
* Added export specifiers.
* Added missing methods.
*
* Revision 1.5  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.4  2002/09/09 18:09:37  grichenk
* implemented GetChoiceObject()
*
* Revision 1.3  2001/05/17 14:58:40  lavr
* Typos corrected
*
* Revision 1.2  2001/01/22 23:20:30  vakatov
* + CObjectInfo::GetMember(), CConstObjectInfo::GetMember()
*
* Revision 1.1  2000/10/20 15:51:26  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* ===========================================================================
*/
