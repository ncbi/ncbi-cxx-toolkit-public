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
* Revision 1.13  2000/10/17 18:45:34  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.12  2000/10/13 16:28:39  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.11  2000/10/03 17:22:43  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.10  2000/09/26 19:24:57  vasilche
* Added user interface for setting read/write/copy hooks.
*
* Revision 1.9  2000/09/26 17:38:21  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.8  2000/09/18 20:00:23  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.7  2000/09/01 13:16:17  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.6  2000/08/15 19:44:47  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.5  2000/07/27 14:59:29  thiessen
* minor fix to avoid Mac compiler error
*
* Revision 1.4  2000/07/06 16:21:19  vasilche
* Added interface to primitive types in CObjectInfo & CConstObjectInfo.
*
* Revision 1.3  2000/07/03 18:42:44  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.2  2000/06/16 16:31:19  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.1  2000/03/29 15:55:27  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/object.hpp>
#include <serial/typeinfo.hpp>
#include <serial/continfo.hpp>
#include <serial/classinfob.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/stdtypes.hpp>
#include <serial/memberid.hpp>
#include <serial/delaybuf.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objhook.hpp>
#include <serial/objcopy.hpp>

BEGIN_NCBI_SCOPE

// object type info

void CObjectTypeInfo::WrongTypeFamily(ETypeFamily /*needFamily*/) const
{
    THROW1_TRACE(runtime_error, "wrong type family");
}

void CConstObjectInfoEI::ReportNonValid(void) const
{
    ERR_POST("CElementIterator was used without checking its validity");
}

void CObjectInfoEI::ReportNonValid(void) const
{
    ERR_POST("CElementIterator was used without checking its validity");
}

void CObjectTypeInfoII::ReportNonValid(void) const
{
    ERR_POST("CTypeMemberIterator is used without validity check");
}

const CPrimitiveTypeInfo* CObjectTypeInfo::GetPrimitiveTypeInfo(void) const
{
    CheckTypeFamily(eTypeFamilyPrimitive);
    return CTypeConverter<CPrimitiveTypeInfo>::SafeCast(GetTypeInfo());
}

const CClassTypeInfo* CObjectTypeInfo::GetClassTypeInfo(void) const
{
    CheckTypeFamily(eTypeFamilyClass);
    return CTypeConverter<CClassTypeInfo>::SafeCast(GetTypeInfo());
}

const CChoiceTypeInfo* CObjectTypeInfo::GetChoiceTypeInfo(void) const
{
    CheckTypeFamily(eTypeFamilyChoice);
    return CTypeConverter<CChoiceTypeInfo>::SafeCast(GetTypeInfo());
}

const CContainerTypeInfo* CObjectTypeInfo::GetContainerTypeInfo(void) const
{
    CheckTypeFamily(eTypeFamilyContainer);
    return CTypeConverter<CContainerTypeInfo>::SafeCast(GetTypeInfo());
}

const CPointerTypeInfo* CObjectTypeInfo::GetPointerTypeInfo(void) const
{
    CheckTypeFamily(eTypeFamilyPointer);
    return CTypeConverter<CPointerTypeInfo>::SafeCast(GetTypeInfo());
}

CObjectTypeInfo CObjectTypeInfo::GetElementType(void) const
{
    return GetContainerTypeInfo()->GetElementType();
}

// pointer interface
CConstObjectInfo CConstObjectInfo::GetPointedObject(void) const
{
    const CPointerTypeInfo* pointerType = GetPointerTypeInfo();
    return pair<TConstObjectPtr, TTypeInfo>(pointerType->GetObjectPointer(GetObjectPtr()), pointerType->GetPointedType());
}

CObjectInfo CObjectInfo::GetPointedObject(void) const
{
    const CPointerTypeInfo* pointerType = GetPointerTypeInfo();
    return pair<TObjectPtr, TTypeInfo>(pointerType->GetObjectPointer(GetObjectPtr()), pointerType->GetPointedType());
}

// primitive interface
EPrimitiveValueType CObjectTypeInfo::GetPrimitiveValueType(void) const
{
    return GetPrimitiveTypeInfo()->GetPrimitiveValueType();
}

bool CObjectTypeInfo::IsPrimitiveValueSigned(void) const
{
    return GetPrimitiveTypeInfo()->IsSigned();
}

TMemberIndex CObjectTypeInfo::FindMemberIndex(const string& name) const
{
    return GetClassTypeInfo()->GetMembers().Find(name);
}

TMemberIndex CObjectTypeInfo::FindMemberIndex(int tag) const
{
    return GetClassTypeInfo()->GetMembers().Find(tag);
}

TMemberIndex CObjectTypeInfo::FindVariantIndex(const string& name) const
{
    return GetChoiceTypeInfo()->GetVariants().Find(name);
}

TMemberIndex CObjectTypeInfo::FindVariantIndex(int tag) const
{
    return GetChoiceTypeInfo()->GetVariants().Find(tag);
}

bool CConstObjectInfo::GetPrimitiveValueBool(void) const
{
    return GetPrimitiveTypeInfo()->GetValueBool(GetObjectPtr());
}

char CConstObjectInfo::GetPrimitiveValueChar(void) const
{
    return GetPrimitiveTypeInfo()->GetValueChar(GetObjectPtr());
}

long CConstObjectInfo::GetPrimitiveValueLong(void) const
{
    return GetPrimitiveTypeInfo()->GetValueLong(GetObjectPtr());
}

unsigned long CConstObjectInfo::GetPrimitiveValueULong(void) const
{
    return GetPrimitiveTypeInfo()->GetValueULong(GetObjectPtr());
}

double CConstObjectInfo::GetPrimitiveValueDouble(void) const
{
    return GetPrimitiveTypeInfo()->GetValueDouble(GetObjectPtr());
}

void CConstObjectInfo::GetPrimitiveValueString(string& value) const
{
    GetPrimitiveTypeInfo()->GetValueString(GetObjectPtr(), value);
}

string CConstObjectInfo::GetPrimitiveValueString(void) const
{
    string value;
    GetPrimitiveValueString(value);
    return value;
}

void CConstObjectInfo::GetPrimitiveValueOctetString(vector<char>& value) const
{
    GetPrimitiveTypeInfo()->GetValueOctetString(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueBool(bool value)
{
    GetPrimitiveTypeInfo()->SetValueBool(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueChar(char value)
{
    GetPrimitiveTypeInfo()->SetValueChar(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueLong(long value)
{
    GetPrimitiveTypeInfo()->SetValueLong(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueULong(unsigned long value)
{
    GetPrimitiveTypeInfo()->SetValueULong(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueDouble(double value)
{
    GetPrimitiveTypeInfo()->SetValueDouble(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueString(const string& value)
{
    GetPrimitiveTypeInfo()->SetValueString(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueOctetString(const vector<char>& value)
{
    GetPrimitiveTypeInfo()->SetValueOctetString(GetObjectPtr(), value);
}

TMemberIndex CConstObjectInfo::GetCurrentChoiceVariantIndex(void) const
{
    return GetChoiceTypeInfo()->GetIndex(GetObjectPtr());
}

void CObjectTypeInfo::SetLocalReadHook(CObjectIStream& stream,
                                       CReadObjectHook* hook) const
{
    GetNCTypeInfo()->SetLocalReadHook(stream, hook);
}

void CObjectTypeInfo::SetGlobalReadHook(CReadObjectHook* hook) const
{
    GetNCTypeInfo()->SetGlobalReadHook(hook);
}

void CObjectTypeInfo::ResetLocalReadHook(CObjectIStream& stream) const
{
    GetNCTypeInfo()->ResetLocalReadHook(stream);
}

void CObjectTypeInfo::ResetGlobalReadHook(void) const
{
    GetNCTypeInfo()->ResetGlobalReadHook();
}

void CObjectTypeInfo::SetLocalWriteHook(CObjectOStream& stream,
                                        CWriteObjectHook* hook) const
{
    GetNCTypeInfo()->SetLocalWriteHook(stream, hook);
}

void CObjectTypeInfo::SetGlobalWriteHook(CWriteObjectHook* hook) const
{
    GetNCTypeInfo()->SetGlobalWriteHook(hook);
}

void CObjectTypeInfo::ResetLocalWriteHook(CObjectOStream& stream) const
{
    GetNCTypeInfo()->ResetLocalWriteHook(stream);
}

void CObjectTypeInfo::ResetGlobalWriteHook(void) const
{
    GetNCTypeInfo()->ResetGlobalWriteHook();
}

void CObjectTypeInfo::SetLocalCopyHook(CObjectStreamCopier& stream,
                                       CCopyObjectHook* hook) const
{
    GetNCTypeInfo()->SetLocalCopyHook(stream, hook);
}

void CObjectTypeInfo::SetGlobalCopyHook(CCopyObjectHook* hook) const
{
    GetNCTypeInfo()->SetGlobalCopyHook(hook);
}

void CObjectTypeInfo::ResetLocalCopyHook(CObjectStreamCopier& stream) const
{
    GetNCTypeInfo()->ResetLocalCopyHook(stream);
}

void CObjectTypeInfo::ResetGlobalCopyHook(void) const
{
    GetNCTypeInfo()->ResetGlobalCopyHook();
}

// container iterators

CConstObjectInfoEI::CConstObjectInfoEI(const CConstObjectInfo& object)
    : m_Iterator(object.GetObjectPtr(), object.GetContainerTypeInfo())
{
    _DEBUG_ARG(m_LastCall = eNone);
}

CConstObjectInfoEI& CConstObjectInfoEI::operator=(const CConstObjectInfo& object)
{
    m_Iterator.Init(object.GetObjectPtr(), object.GetContainerTypeInfo());
    _DEBUG_ARG(m_LastCall = eNone);
    return *this;
}

CObjectInfoEI::CObjectInfoEI(const CObjectInfo& object)
    : m_Iterator(object.GetObjectPtr(), object.GetContainerTypeInfo())
{
    _DEBUG_ARG(m_LastCall = eNone);
}

CObjectInfoEI& CObjectInfoEI::operator=(const CObjectInfo& object)
{
    m_Iterator.Init(object.GetObjectPtr(), object.GetContainerTypeInfo());
    _DEBUG_ARG(m_LastCall = eNone);
    return *this;
}

// class iterators

bool CObjectTypeInfoMI::IsSet(const CConstObjectInfo& object) const
{
    const CMemberInfo* memberInfo = GetMemberInfo();
    if ( memberInfo->HaveSetFlag() )
        return memberInfo->GetSetFlag(object.GetObjectPtr());
    
    if ( memberInfo->CanBeDelayed() &&
         memberInfo->GetDelayBuffer(object.GetObjectPtr()).Delayed() )
        return true;

    if ( memberInfo->Optional() ) {
        TConstObjectPtr defaultPtr = memberInfo->GetDefault();
        TConstObjectPtr memberPtr =
            memberInfo->GetMemberPtr(object.GetObjectPtr());
        TTypeInfo memberType = memberInfo->GetTypeInfo();
        if ( !defaultPtr ) {
            if ( memberType->IsDefault(memberPtr) )
                return false; // DEFAULT
        }
        else {
            if ( memberType->Equals(memberPtr, defaultPtr) )
                return false; // OPTIONAL
        }
    }
    return true;
}

void CObjectTypeInfoMI::SetLocalReadHook(CObjectIStream& stream,
                                         CReadClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetLocalReadHook(stream, hook);
}

void CObjectTypeInfoMI::SetGlobalReadHook(CReadClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetGlobalReadHook(hook);
}

void CObjectTypeInfoMI::ResetLocalReadHook(CObjectIStream& stream) const
{
    GetNCMemberInfo()->ResetLocalReadHook(stream);
}

void CObjectTypeInfoMI::ResetGlobalReadHook(void) const
{
    GetNCMemberInfo()->ResetGlobalReadHook();
}

void CObjectTypeInfoMI::SetLocalWriteHook(CObjectOStream& stream,
                                          CWriteClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetLocalWriteHook(stream, hook);
}

void CObjectTypeInfoMI::SetGlobalWriteHook(CWriteClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetGlobalWriteHook(hook);
}

void CObjectTypeInfoMI::ResetLocalWriteHook(CObjectOStream& stream) const
{
    GetNCMemberInfo()->ResetLocalWriteHook(stream);
}

void CObjectTypeInfoMI::ResetGlobalWriteHook(void) const
{
    GetNCMemberInfo()->ResetGlobalWriteHook();
}

void CObjectTypeInfoMI::SetLocalCopyHook(CObjectStreamCopier& stream,
                                         CCopyClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetLocalCopyHook(stream, hook);
}

void CObjectTypeInfoMI::SetGlobalCopyHook(CCopyClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetGlobalCopyHook(hook);
}

void CObjectTypeInfoMI::ResetLocalCopyHook(CObjectStreamCopier& stream) const
{
    GetNCMemberInfo()->ResetLocalCopyHook(stream);
}

void CObjectTypeInfoMI::ResetGlobalCopyHook(void) const
{
    GetNCMemberInfo()->ResetGlobalCopyHook();
}

pair<TConstObjectPtr, TTypeInfo> CConstObjectInfoMI::GetMemberPair(void) const
{
    const CMemberInfo* memberInfo = GetMemberInfo();
    return make_pair(memberInfo->GetMemberPtr(m_Object.GetObjectPtr()),
                     memberInfo->GetTypeInfo());
}

pair<TObjectPtr, TTypeInfo> CObjectInfoMI::GetMemberPair(void) const
{
    const CMemberInfo* memberInfo = GetMemberInfo();
    return make_pair(memberInfo->GetMemberPtr(m_Object.GetObjectPtr()),
                     memberInfo->GetTypeInfo());
}

void CObjectInfoMI::Erase(void)
{
    const CMemberInfo* mInfo = GetMemberInfo();
    if ( !mInfo->Optional() || mInfo->GetDefault() )
        THROW1_TRACE(runtime_error, "cannot reset non OPTIONAL member");
    
    TObjectPtr objectPtr = m_Object.GetObjectPtr();
    // check 'set' flag
    bool haveSetFlag = mInfo->HaveSetFlag();
    if ( haveSetFlag && !mInfo->GetSetFlag(objectPtr) ) {
        // member not set
        return;
    }

    // reset member
    mInfo->GetTypeInfo()->SetDefault(mInfo->GetMemberPtr(objectPtr));

    // update 'set' flag
    if ( haveSetFlag )
        mInfo->GetSetFlag(objectPtr) = false;
}

// choice iterators

void CObjectTypeInfoVI::SetLocalReadHook(CObjectIStream& stream,
                                         CReadChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalReadHook(stream, hook);
}

void CObjectTypeInfoVI::SetGlobalReadHook(CReadChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalReadHook(hook);
}

void CObjectTypeInfoVI::ResetLocalReadHook(CObjectIStream& stream) const
{
    GetNCVariantInfo()->ResetLocalReadHook(stream);
}

void CObjectTypeInfoVI::ResetGlobalReadHook(void) const
{
    GetNCVariantInfo()->ResetGlobalReadHook();
}

void CObjectTypeInfoVI::SetLocalWriteHook(CObjectOStream& stream,
                                          CWriteChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalWriteHook(stream, hook);
}

void CObjectTypeInfoVI::SetGlobalWriteHook(CWriteChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalWriteHook(hook);
}

void CObjectTypeInfoVI::ResetLocalWriteHook(CObjectOStream& stream) const
{
    GetNCVariantInfo()->ResetLocalWriteHook(stream);
}

void CObjectTypeInfoVI::ResetGlobalWriteHook(void) const
{
    GetNCVariantInfo()->ResetGlobalWriteHook();
}

void CObjectTypeInfoVI::SetLocalCopyHook(CObjectStreamCopier& stream,
                                         CCopyChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalCopyHook(stream, hook);
}

void CObjectTypeInfoVI::SetGlobalCopyHook(CCopyChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalCopyHook(hook);
}

void CObjectTypeInfoVI::ResetLocalCopyHook(CObjectStreamCopier& stream) const
{
    GetNCVariantInfo()->ResetLocalCopyHook(stream);
}

void CObjectTypeInfoVI::ResetGlobalCopyHook(void) const
{
    GetNCVariantInfo()->ResetGlobalCopyHook();
}

void CObjectTypeInfoCV::SetLocalReadHook(CObjectIStream& stream,
                                         CReadChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalReadHook(stream, hook);
}

void CObjectTypeInfoCV::SetGlobalReadHook(CReadChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalReadHook(hook);
}

void CObjectTypeInfoCV::ResetLocalReadHook(CObjectIStream& stream) const
{
    GetNCVariantInfo()->ResetLocalReadHook(stream);
}

void CObjectTypeInfoCV::ResetGlobalReadHook(void) const
{
    GetNCVariantInfo()->ResetGlobalReadHook();
}

void CObjectTypeInfoCV::SetLocalWriteHook(CObjectOStream& stream,
                                          CWriteChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalWriteHook(stream, hook);
}

void CObjectTypeInfoCV::SetGlobalWriteHook(CWriteChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalWriteHook(hook);
}

void CObjectTypeInfoCV::ResetLocalWriteHook(CObjectOStream& stream) const
{
    GetNCVariantInfo()->ResetLocalWriteHook(stream);
}

void CObjectTypeInfoCV::ResetGlobalWriteHook(void) const
{
    GetNCVariantInfo()->ResetGlobalWriteHook();
}

void CObjectTypeInfoCV::SetLocalCopyHook(CObjectStreamCopier& stream,
                                         CCopyChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalCopyHook(stream, hook);
}

void CObjectTypeInfoCV::SetGlobalCopyHook(CCopyChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalCopyHook(hook);
}

void CObjectTypeInfoCV::ResetLocalCopyHook(CObjectStreamCopier& stream) const
{
    GetNCVariantInfo()->ResetLocalCopyHook(stream);
}

void CObjectTypeInfoCV::ResetGlobalCopyHook(void) const
{
    GetNCVariantInfo()->ResetGlobalCopyHook();
}

pair<TConstObjectPtr, TTypeInfo> CConstObjectInfoCV::GetVariantPair(void) const
{
    const CVariantInfo* variantInfo = GetVariantInfo();
    return make_pair(variantInfo->GetVariantPtr(m_Object.GetObjectPtr()),
                     variantInfo->GetTypeInfo());
}

pair<TObjectPtr, TTypeInfo> CObjectInfoCV::GetVariantPair(void) const
{
    const CVariantInfo* variantInfo = GetVariantInfo();
    return make_pair(variantInfo->GetVariantPtr(m_Object.GetObjectPtr()),
                     variantInfo->GetTypeInfo());
}

// readers
void CObjectInfo::ReadContainer(CObjectIStream& in,
                                CReadContainerElementHook& hook)
{
    const CContainerTypeInfo* containerType = GetContainerTypeInfo();
    BEGIN_OBJECT_FRAME_OF2(in, eFrameArray, containerType);
    in.BeginContainer(containerType);

    TTypeInfo elementType = containerType->GetElementType();
    BEGIN_OBJECT_FRAME_OF2(in, eFrameArrayElement, elementType);

    while ( in.BeginContainerElement(elementType) ) {
        hook.ReadContainerElement(in, *this);
        in.EndContainerElement();
    }

    END_OBJECT_FRAME_OF(in);

    in.EndContainer();
    END_OBJECT_FRAME_OF(in);
}

class CCObjectClassInfo : public CVoidTypeInfo
{
    typedef CTypeInfo CParent;
public:
    virtual bool IsParentClassOf(const CClassTypeInfo* classInfo) const;
};

TTypeInfo CObjectGetTypeInfo::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CCObjectClassInfo;
    return typeInfo;
}

bool CCObjectClassInfo::IsParentClassOf(const CClassTypeInfo* classInfo) const
{
    return classInfo->IsCObject();
}

END_NCBI_SCOPE
