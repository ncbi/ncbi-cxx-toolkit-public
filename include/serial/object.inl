#if defined(OBJECT__HPP)  &&  !defined(OBJECT__INL)
#define OBJECT__INL

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
* Revision 1.6  2000/09/01 13:16:00  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.5  2000/08/15 19:44:39  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.4  2000/07/06 16:21:16  vasilche
* Added interface to primitive types in CObjectInfo & CConstObjectInfo.
*
* Revision 1.3  2000/07/03 18:42:35  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.2  2000/06/16 16:31:06  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.1  2000/03/29 15:55:20  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* ===========================================================================
*/

/////////////////////////////////////////////////////////////////////////////
// CObjectTypeInfo
/////////////////////////////////////////////////////////////////////////////

inline
CObjectTypeInfo::CObjectTypeInfo(TTypeInfo typeinfo)
    : m_TypeInfo(typeinfo)
{
}

inline
TTypeInfo CObjectTypeInfo::GetTypeInfo(void) const
{
    return m_TypeInfo;
}

inline
CTypeInfo::ETypeFamily CObjectTypeInfo::GetTypeFamily(void) const
{
    return GetTypeInfo()->GetTypeFamily();
}

inline
void CObjectTypeInfo::ResetTypeInfo(void)
{
    m_TypeInfo = 0;
}

inline
void CObjectTypeInfo::SetTypeInfo(TTypeInfo typeinfo)
{
    m_TypeInfo = typeinfo;
}

inline
const CPrimitiveTypeInfo* CObjectTypeInfo::GetPrimitiveTypeInfo(void) const
{
    _ASSERT(dynamic_cast<const CPrimitiveTypeInfo*>(GetTypeInfo()));
    return static_cast<const CPrimitiveTypeInfo*>(GetTypeInfo());
}

inline
const CClassTypeInfoBase* CObjectTypeInfo::GetClassTypeInfoBase(void) const
{
    _ASSERT(dynamic_cast<const CClassTypeInfoBase*>(GetTypeInfo()));
    return static_cast<const CClassTypeInfoBase*>(GetTypeInfo());
}

inline
const CClassTypeInfo* CObjectTypeInfo::GetClassTypeInfo(void) const
{
    _ASSERT(dynamic_cast<const CClassTypeInfo*>(GetTypeInfo()));
    return static_cast<const CClassTypeInfo*>(GetTypeInfo());
}

inline
const CChoiceTypeInfo* CObjectTypeInfo::GetChoiceTypeInfo(void) const
{
    _ASSERT(dynamic_cast<const CChoiceTypeInfo*>(GetTypeInfo()));
    return static_cast<const CChoiceTypeInfo*>(GetTypeInfo());
}

inline
const CContainerTypeInfo* CObjectTypeInfo::GetContainerTypeInfo(void) const
{
    _ASSERT(dynamic_cast<const CContainerTypeInfo*>(GetTypeInfo()));
    return static_cast<const CContainerTypeInfo*>(GetTypeInfo());
}

inline
const CPointerTypeInfo* CObjectTypeInfo::GetPointerTypeInfo(void) const
{
    _ASSERT(dynamic_cast<const CPointerTypeInfo*>(GetTypeInfo()));
    return static_cast<const CPointerTypeInfo*>(GetTypeInfo());
}

inline
CObjectTypeInfo CObjectTypeInfo::GetElementType(void) const
{
    return GetContainerTypeInfo()->GetElementType();
}

// pointer interface
inline
CConstObjectInfo CConstObjectInfo::GetPointedObject(void) const
{
    return GetPointerTypeInfo()->GetPointedObject(*this);
}

inline
CObjectInfo CObjectInfo::GetPointedObject(void) const
{
    return GetPointerTypeInfo()->GetPointedObject(*this);
}

// primitive interface
inline
CPrimitiveTypeInfo::EValueType
CObjectTypeInfo::GetPrimitiveValueType(void) const
{
    return GetPrimitiveTypeInfo()->GetValueType();
}

inline
bool CObjectTypeInfo::IsPrimitiveValueSigned(void) const
{
    return GetPrimitiveTypeInfo()->IsSigned();
}

inline
TMemberIndex CObjectTypeInfo::FindMemberIndex(const string& name) const
{
    return GetClassTypeInfoBase()->GetMembers().FindMember(name);
}

inline
TMemberIndex CObjectTypeInfo::FindMemberIndex(int tag) const
{
    return GetClassTypeInfoBase()->GetMembers().FindMember(tag);
}

/////////////////////////////////////////////////////////////////////////////
// CConstObjectInfo
/////////////////////////////////////////////////////////////////////////////

inline
CConstObjectInfo::CConstObjectInfo(void)
    : m_ObjectPtr(0)
{
}

inline
CConstObjectInfo::CConstObjectInfo(TConstObjectPtr objectPtr,
                                   TTypeInfo typeInfo)
    : CObjectTypeInfo(typeInfo), m_ObjectPtr(objectPtr),
      m_Ref(typeInfo->GetCObjectPtr(objectPtr))
{
}

inline
CConstObjectInfo::CConstObjectInfo(TConstObjectPtr objectPtr,
                                   TTypeInfo typeInfo,
                                   ENonCObject)
    : CObjectTypeInfo(typeInfo), m_ObjectPtr(objectPtr)
{
    _ASSERT(!typeInfo->IsCObject() ||
            static_cast<const CObject*>(objectPtr)->Referenced() ||
            !static_cast<const CObject*>(objectPtr)->CanBeDeleted());
}

inline
CConstObjectInfo::CConstObjectInfo(pair<TConstObjectPtr, TTypeInfo> object)
    : CObjectTypeInfo(object.second), m_ObjectPtr(object.first),
      m_Ref(object.second->GetCObjectPtr(object.first))
{
}

inline
CConstObjectInfo::CConstObjectInfo(pair<TObjectPtr, TTypeInfo> object)
    : CObjectTypeInfo(object.second), m_ObjectPtr(object.first),
      m_Ref(object.second->GetCObjectPtr(object.first))
{
}

inline
bool CConstObjectInfo::Valid(void) const
{
    return m_ObjectPtr != 0;
}

inline
CConstObjectInfo::operator bool(void) const
{
    return Valid();
}

inline
void CConstObjectInfo::ResetObjectPtr(void)
{
    m_ObjectPtr = 0;
}

inline
TConstObjectPtr CConstObjectInfo::GetObjectPtr(void) const
{
    return m_ObjectPtr;
}

inline
pair<TConstObjectPtr, TTypeInfo> CConstObjectInfo::GetPair(void) const
{
    return make_pair(GetObjectPtr(), GetTypeInfo());
}

inline
void CConstObjectInfo::Reset(void)
{
    m_ObjectPtr = 0;
    ResetTypeInfo();
    m_Ref.Reset();
}

inline
void CConstObjectInfo::Set(TConstObjectPtr objectPtr, TTypeInfo typeInfo)
{
    m_ObjectPtr = objectPtr;
    SetTypeInfo(typeInfo);
    m_Ref.Reset(typeInfo->GetCObjectPtr(objectPtr));
}

inline
CConstObjectInfo&
CConstObjectInfo::operator=(pair<TConstObjectPtr, TTypeInfo> object)
{
    Set(object.first, object.second);
    return *this;
}

inline
CConstObjectInfo&
CConstObjectInfo::operator=(pair<TObjectPtr, TTypeInfo> object)
{
    Set(object.first, object.second);
    return *this;
}

inline
bool CConstObjectInfo::GetPrimitiveValueBool(void) const
{
    return GetPrimitiveTypeInfo()->GetValueBool(GetObjectPtr());
}

inline
char CConstObjectInfo::GetPrimitiveValueChar(void) const
{
    return GetPrimitiveTypeInfo()->GetValueChar(GetObjectPtr());
}

inline
long CConstObjectInfo::GetPrimitiveValueLong(void) const
{
    return GetPrimitiveTypeInfo()->GetValueLong(GetObjectPtr());
}

inline
unsigned long CConstObjectInfo::GetPrimitiveValueULong(void) const
{
    return GetPrimitiveTypeInfo()->GetValueULong(GetObjectPtr());
}

inline
double CConstObjectInfo::GetPrimitiveValueDouble(void) const
{
    return GetPrimitiveTypeInfo()->GetValueDouble(GetObjectPtr());
}

inline
void CConstObjectInfo::GetPrimitiveValueString(string& value) const
{
    GetPrimitiveTypeInfo()->GetValueString(GetObjectPtr(), value);
}

inline
string CConstObjectInfo::GetPrimitiveValueString(void) const
{
    string value;
    GetPrimitiveValueString(value);
    return value;
}

inline
void CConstObjectInfo::GetPrimitiveValueOctetString(vector<char>& value) const
{
    GetPrimitiveTypeInfo()->GetValueOctetString(GetObjectPtr(), value);
}

inline
TMemberIndex CConstObjectInfo::GetCurrentChoiceVariantIndex(void) const
{
    return GetChoiceTypeInfo()->GetIndex(GetObjectPtr());
}

/////////////////////////////////////////////////////////////////////////////
// CObjectInfo
/////////////////////////////////////////////////////////////////////////////

inline
CObjectInfo::CObjectInfo(void)
{
}

inline
CObjectInfo::CObjectInfo(TTypeInfo typeInfo)
    : CParent(typeInfo->Create(), typeInfo)
{
}

inline
CObjectInfo::CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo)
    : CParent(objectPtr, typeInfo)
{
}

inline
CObjectInfo::CObjectInfo(TObjectPtr objectPtr,
                         TTypeInfo typeInfo,
                         ENonCObject nonCObject)
    : CParent(objectPtr, typeInfo, nonCObject)
{
}

inline
CObjectInfo::CObjectInfo(pair<TObjectPtr, TTypeInfo> object)
    : CParent(object)
{
}

inline
TObjectPtr CObjectInfo::GetObjectPtr(void) const
{
    return const_cast<TObjectPtr>(CParent::GetObjectPtr());
}

inline
pair<TObjectPtr, TTypeInfo> CObjectInfo::GetPair(void) const
{
    return make_pair(GetObjectPtr(), GetTypeInfo());
}

inline
CObjectInfo&
CObjectInfo::operator=(pair<TObjectPtr, TTypeInfo> object)
{
    Set(object.first, object.second);
    return *this;
}

inline
void CObjectInfo::SetPrimitiveValueBool(bool value)
{
    GetPrimitiveTypeInfo()->SetValueBool(GetObjectPtr(), value);
}

inline
void CObjectInfo::SetPrimitiveValueChar(char value)
{
    GetPrimitiveTypeInfo()->SetValueChar(GetObjectPtr(), value);
}

inline
void CObjectInfo::SetPrimitiveValueLong(long value)
{
    GetPrimitiveTypeInfo()->SetValueLong(GetObjectPtr(), value);
}

inline
void CObjectInfo::SetPrimitiveValueULong(unsigned long value)
{
    GetPrimitiveTypeInfo()->SetValueULong(GetObjectPtr(), value);
}

inline
void CObjectInfo::SetPrimitiveValueDouble(double value)
{
    GetPrimitiveTypeInfo()->SetValueDouble(GetObjectPtr(), value);
}

inline
void CObjectInfo::SetPrimitiveValueString(const string& value)
{
    GetPrimitiveTypeInfo()->SetValueString(GetObjectPtr(), value);
}

inline
void CObjectInfo::SetPrimitiveValueOctetString(const vector<char>& value)
{
    GetPrimitiveTypeInfo()->SetValueOctetString(GetObjectPtr(), value);
}

/////////////////////////////////////////////////////////////////////////////
// iterators
/////////////////////////////////////////////////////////////////////////////

// container interface

inline
CConstObjectInfoEI::CConstObjectInfoEI(void)
{
    _DEBUG_ARG(m_LastCall = eNone);
}

inline
bool CConstObjectInfoEI::CheckValid(void) const
{
#if _DEBUG
    if ( m_LastCall != eValid)
        ERR_POST("CElementIterator was used without checking its validity");
#endif
    return m_Iterator.Valid();
}

inline
bool CConstObjectInfoEI::Valid(void) const
{
    _DEBUG_ARG(m_LastCall = eValid);
    return CheckValid();
}

inline
CConstObjectInfoEI::operator bool(void) const
{
    return Valid();
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

inline
CObjectInfoEI::CObjectInfoEI(void)
{
    _DEBUG_ARG(m_LastCall = eNone);
}

inline
bool CObjectInfoEI::CheckValid(void) const
{
#if _DEBUG
    if ( m_LastCall != eValid)
        ERR_POST("CElementIterator was used without checking its validity");
#endif
    return m_Iterator.Valid();
}

inline
bool CObjectInfoEI::Valid(void) const
{
    _DEBUG_ARG(m_LastCall = eValid);
    return CheckValid();
}

inline
CObjectInfoEI::operator bool(void) const
{
    return Valid();
}

inline
void CObjectInfoEI::Next(void)
{
    _ASSERT(CheckValid());
    _DEBUG_ARG(m_LastCall = eNext);
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
    _DEBUG_ARG(m_LastCall = eErase);
    m_Iterator.Erase();
}

// class interface

inline
CObjectTypeInfoMI::CObjectTypeInfoMI(void)
    : m_ClassTypeInfoBase(0),
      m_MemberIndex(kFirstMemberIndex),
      m_LastMemberIndex(kInvalidMember)
{
    _DEBUG_ARG(m_LastCall = eNone);
}

inline
void CObjectTypeInfoMI::Init(const CObjectTypeInfo& info)
{
    const CClassTypeInfoBase* classType =
        m_ClassTypeInfoBase = info.GetClassTypeInfoBase();
    const CMembersInfo& members = classType->GetMembers();
    m_MemberIndex = members.FirstMemberIndex();
    m_LastMemberIndex = members.LastMemberIndex();
    _DEBUG_ARG(m_LastCall = eNone);
}

inline
void CObjectTypeInfoMI::Init(const CObjectTypeInfo& info,
                                            TMemberIndex index)
{
    const CClassTypeInfoBase* classType =
        m_ClassTypeInfoBase = info.GetClassTypeInfoBase();
    const CMembersInfo& members = classType->GetMembers();
    m_MemberIndex = index;
    m_LastMemberIndex = members.LastMemberIndex();
    _DEBUG_ARG(m_LastCall = eNone);
}

inline
CObjectTypeInfoMI::CObjectTypeInfoMI(const CObjectTypeInfo& info)
{
    Init(info);
}

inline
CObjectTypeInfoMI::CObjectTypeInfoMI(const CObjectTypeInfo& info,
                                     TMemberIndex index)
{
    Init(info, index);
}

inline
const CClassTypeInfoBase* CObjectTypeInfoMI::GetClassTypeInfoBase(void) const
{
    return m_ClassTypeInfoBase;
}

inline
const CMemberInfo* CObjectTypeInfoMI::GetMemberInfo(void) const
{
    return GetClassTypeInfoBase()->GetMemberInfo(GetMemberIndex());
}

inline
const string& CObjectTypeInfoMI::GetAlias(void) const
{
    return GetMemberInfo()->GetId().GetName();
}

inline
bool CObjectTypeInfoMI::CheckValid(void) const
{
#if _DEBUG
    if ( m_LastCall != eValid)
        ERR_POST("CTypeMemberIterator is used without validity check");
#endif
    return m_MemberIndex >= kFirstMemberIndex &&
        m_MemberIndex <= m_LastMemberIndex;
}

inline
TMemberIndex CObjectTypeInfoMI::GetMemberIndex(void) const
{
    _ASSERT(CheckValid());
    return m_MemberIndex;
}

inline
bool CObjectTypeInfoMI::Valid(void) const
{
    _DEBUG_ARG(m_LastCall = eValid);
    return CheckValid();
}

inline
CObjectTypeInfoMI::operator bool(void) const
{
    return Valid();
}

inline
bool CObjectTypeInfoMI::Optional(void) const
{
    return GetMemberInfo()->Optional();
}

inline
void CObjectTypeInfoMI::Next(void)
{
    _ASSERT(CheckValid());
    _DEBUG_ARG(m_LastCall = eNext);
    ++m_MemberIndex;
}

inline
CObjectTypeInfoMI& CObjectTypeInfoMI::operator++(void)
{
    Next();
    return *this;
}

inline
bool CObjectTypeInfoMI::operator==(const CObjectTypeInfoMI& iter) const
{
    _ASSERT(GetClassTypeInfoBase() == iter.GetClassTypeInfoBase());
    return GetMemberIndex() == iter.GetMemberIndex();
}

inline
bool CObjectTypeInfoMI::operator!=(const CObjectTypeInfoMI& iter) const
{
    _ASSERT(GetClassTypeInfoBase() == iter.GetClassTypeInfoBase());
    return GetMemberIndex() != iter.GetMemberIndex();
}

inline
CObjectTypeInfo CObjectTypeInfoMI::GetClassType(void) const
{
    return GetClassTypeInfoBase();
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
    _ASSERT(m_VariantIndex <= choiceInfo->GetMembers().LastMemberIndex());
}

inline
CObjectTypeInfoCV::CObjectTypeInfoCV(const CObjectTypeInfo& info,
                                     TMemberIndex index)
{
    const CChoiceTypeInfo* choiceInfo =
        m_ChoiceTypeInfo = info.GetChoiceTypeInfo();
    if ( index > choiceInfo->GetMembers().LastMemberIndex() )
        index = kEmptyChoice;
    else {
        _ASSERT(index >= kEmptyChoice);
    }
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
CObjectTypeInfoCV::operator bool(void) const
{
    return Valid();
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
const CMemberInfo* CObjectTypeInfoCV::GetVariantInfo(void) const
{
    return GetChoiceTypeInfo()->GetMemberInfo(GetVariantIndex());
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
CObjectInfoCV& CObjectInfoCV::operator=(const CObjectInfo& object)
{
    CParent::Init(object);
    m_Object = object;
    return *this;
}

/////////////////////////////////////////////////////////////////////////////
// iterator getters
/////////////////////////////////////////////////////////////////////////////

// container interface

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

#endif /* def OBJECT__HPP  &&  ndef OBJECT__INL */
