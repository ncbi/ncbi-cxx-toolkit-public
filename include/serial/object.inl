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

// class interface
inline
const CMemberInfo* CObjectTypeInfo::GetMemberInfo(TMemberIndex index) const
{
    return GetClassTypeInfoBase()->GetMemberInfo(index);
}

inline
TMemberIndex CObjectTypeInfo::FindMember(const string& name) const
{
    return GetClassTypeInfoBase()->GetMembers().FindMember(name);
}

inline
TMemberIndex CObjectTypeInfo::FindMemberByTag(int tag) const
{
    return GetClassTypeInfoBase()->GetMembers().FindMember(tag);
}

inline
TMemberIndex CConstObjectInfo::WhichChoice(void) const
{
    return GetChoiceTypeInfo()->GetIndex(GetObjectPtr());
}

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
const CMemberInfo* CObjectTypeInfo::GetMemberInfo(const string& memberName) const
{
    return GetMemberInfo(FindMember(memberName));
}

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
CObjectInfo::CObjectInfo(void)
{
}

inline
CObjectInfo::CObjectInfo(TTypeInfo typeInfo)
    : CConstObjectInfo(typeInfo->Create(), typeInfo)
{
}

inline
CObjectInfo::CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo)
    : CConstObjectInfo(objectPtr, typeInfo)
{
}

inline
CObjectInfo::CObjectInfo(TObjectPtr objectPtr,
                         TTypeInfo typeInfo,
                         ENonCObject nonCObject)
    : CConstObjectInfo(objectPtr, typeInfo, nonCObject)
{
}

inline
CObjectInfo::CObjectInfo(pair<TObjectPtr, TTypeInfo> object)
    : CConstObjectInfo(object)
{
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

#endif /* def OBJECT__HPP  &&  ndef OBJECT__INL */
