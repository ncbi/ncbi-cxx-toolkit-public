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
* Revision 1.10  2000/10/03 17:22:33  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.9  2000/09/26 19:24:54  vasilche
* Added user interface for setting read/write/copy hooks.
*
* Revision 1.8  2000/09/26 17:38:07  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.7  2000/09/18 20:00:04  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
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
CTypeInfo* CObjectTypeInfo::GetNCTypeInfo(void) const
{
    return const_cast<CTypeInfo*>(GetTypeInfo());
}

inline
ETypeFamily CObjectTypeInfo::GetTypeFamily(void) const
{
    return GetTypeInfo()->GetTypeFamily();
}

inline
void CObjectTypeInfo::CheckTypeFamily(ETypeFamily family) const
{
    if ( GetTypeInfo()->GetTypeFamily() != family )
        WrongTypeFamily(family);
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
bool CObjectTypeInfo::Valid(void) const
{
    return m_TypeInfo != 0;
}

inline
CObjectTypeInfo::operator bool(void) const
{
    return Valid();
}

inline
bool CObjectTypeInfo::operator!(void) const
{
    return !Valid();
}

inline
bool CObjectTypeInfo::operator==(const CObjectTypeInfo& type) const
{
    return GetTypeInfo() == type.GetTypeInfo();
}

inline
bool CObjectTypeInfo::operator!=(const CObjectTypeInfo& type) const
{
    return GetTypeInfo() != type.GetTypeInfo();
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
bool CConstObjectInfo::operator!(void) const
{
    return !Valid();
}

inline
void CConstObjectInfo::ResetObjectPtr(void)
{
    m_ObjectPtr = 0;
    m_Ref.Reset();
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
    ResetObjectPtr();
    ResetTypeInfo();
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
        ReportNonValid();
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
bool CConstObjectInfoEI::operator!(void) const
{
    return !Valid();
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
    _DEBUG_ARG(m_LastCall = eNone);
}

inline
bool CObjectInfoEI::CheckValid(void) const
{
#if _DEBUG
    if ( m_LastCall != eValid)
        ReportNonValid();
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
bool CObjectInfoEI::operator!(void) const
{
    return !Valid();
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

/*
inline
CObjectInfo CObjectInfoEI::operator->(void) const
{
    _ASSERT(CheckValid());
    return m_Iterator.Get();
}
*/

inline
void CObjectInfoEI::Erase(void)
{
    _ASSERT(CheckValid());
    _DEBUG_ARG(m_LastCall = eErase);
    m_Iterator.Erase();
}

// class interface

inline
CObjectTypeInfoII::CObjectTypeInfoII(void)
    : m_ItemIndex(kFirstMemberIndex),
      m_LastItemIndex(kInvalidMember)
{
    _DEBUG_ARG(m_LastCall = eNone);
}

inline
void CObjectTypeInfoII::Init(const CClassTypeInfoBase* typeInfo,
                             TMemberIndex index)
{
    m_OwnerType = typeInfo;
    m_ItemIndex = index;
    m_LastItemIndex = typeInfo->GetItems().LastIndex();
    _DEBUG_ARG(m_LastCall = eNone);
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
#if _DEBUG
    if ( m_LastCall != eValid)
        ReportNonValid();
#endif
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
    _DEBUG_ARG(m_LastCall = eValid);
    return CheckValid();
}

inline
CObjectTypeInfoII::operator bool(void) const
{
    return Valid();
}

inline
bool CObjectTypeInfoII::operator!(void) const
{
    return !Valid();
}

inline
void CObjectTypeInfoII::Next(void)
{
    _ASSERT(CheckValid());
    _DEBUG_ARG(m_LastCall = eNext);
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

/*
inline
CConstObjectInfo CConstObjectInfoMI::operator->(void) const
{
    return GetMemberPair();
}
*/

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

/*
inline
CObjectInfo CObjectInfoMI::operator->(void) const
{
    return GetMemberPair();
}
*/

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
bool CObjectTypeInfoCV::operator!(void) const
{
    return !Valid();
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

/*
inline
CConstObjectInfo CConstObjectInfoCV::operator->(void) const
{
    return GetVariantPair();
}
*/

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

/*
inline
CObjectInfo CObjectInfoCV::operator->(void) const
{
    return GetVariantPair();
}
*/

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
