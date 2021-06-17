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

#include <ncbi_pch.hpp>
#include <serial/impl/ptrinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serialutil.hpp>

BEGIN_NCBI_SCOPE

CPointerTypeInfo::CPointerTypeInfo(TTypeInfo type)
    : CParent(eTypeFamilyPointer, sizeof(TObjectPtr)), m_DataTypeRef(type)
{
    InitPointerTypeInfoFunctions();
}

CPointerTypeInfo::CPointerTypeInfo(const CTypeRef& typeRef)
    : CParent(eTypeFamilyPointer, sizeof(TObjectPtr)), m_DataTypeRef(typeRef)
{
    InitPointerTypeInfoFunctions();
}

CPointerTypeInfo::CPointerTypeInfo(size_t size, TTypeInfo type)
    : CParent(eTypeFamilyPointer, size), m_DataTypeRef(type)
{
    InitPointerTypeInfoFunctions();
}

CPointerTypeInfo::CPointerTypeInfo(size_t size, const CTypeRef& typeRef)
    : CParent(eTypeFamilyPointer, size), m_DataTypeRef(typeRef)
{
    InitPointerTypeInfoFunctions();
}

CPointerTypeInfo::CPointerTypeInfo(const string& name, TTypeInfo type)
    : CParent(eTypeFamilyPointer, sizeof(TObjectPtr), name),
      m_DataTypeRef(type)
{
    InitPointerTypeInfoFunctions();
}

CPointerTypeInfo::CPointerTypeInfo(const string& name, size_t size, TTypeInfo type)
    : CParent(eTypeFamilyPointer, size, name),
      m_DataTypeRef(type)
{
    InitPointerTypeInfoFunctions();
    CopyInfoItem(type);
}

void CPointerTypeInfo::InitPointerTypeInfoFunctions(void)
{
    SetCreateFunction(&CreatePointer);
    SetReadFunction(&ReadPointer);
    SetWriteFunction(&WritePointer);
    SetCopyFunction(&CopyPointer);
    SetSkipFunction(&SkipPointer);
    SetFunctions(&GetPointer, &SetPointer);
}

void CPointerTypeInfo::SetFunctions(TGetDataFunction getFunc,
                                    TSetDataFunction setFunc)
{
    m_GetData = getFunc;
    m_SetData = setFunc;
}

CTypeInfo* CPointerTypeInfo::SetTag(
    CAsnBinaryDefs::TLongTag tag,
    CAsnBinaryDefs::ETagClass tagclass,CAsnBinaryDefs::ETagType tagtype)
{
    CParent::SetTag(tag, tagclass, tagtype);
    if (tagtype == CAsnBinaryDefs::eImplicit) {
        const CPointerTypeInfo *ptrtype = this;
        while (ptrtype) {
            TTypeInfo ptype = ptrtype->GetPointedType();
            ptrtype = dynamic_cast<const CPointerTypeInfo*>(ptype);
            if (ptrtype) {
                if (ptrtype->GetTagType() == CAsnBinaryDefs::eImplicit ||
                    !ptrtype->HasTag()) {
                    continue;
                }
                m_TagConstructed = CAsnBinaryDefs::eConstructed;
            } else {
                m_TagConstructed = ptype->GetTagConstructed();
            }
            break;
        }
    } else {
        m_TagConstructed = CAsnBinaryDefs::eConstructed;
    }
    return this;
}

TTypeInfo CPointerTypeInfo::GetTypeInfo(TTypeInfo base)
{
    return new CPointerTypeInfo(base);
}

CTypeInfo::EMayContainType
CPointerTypeInfo::GetMayContainType(TTypeInfo type) const
{
    return GetPointedType()->IsOrMayContainType(type);
}

TTypeInfo CPointerTypeInfo::GetRealDataTypeInfo(TConstObjectPtr object) const
{
    TTypeInfo dataTypeInfo = GetPointedType();
    if ( object )
        dataTypeInfo = dataTypeInfo->GetRealTypeInfo(object);
    return dataTypeInfo;
}

TObjectPtr CPointerTypeInfo::GetPointer(const CPointerTypeInfo* /*objectType*/,
                                        TObjectPtr objectPtr)
{
    return CTypeConverter<TObjectPtr>::Get(objectPtr);
}

void CPointerTypeInfo::SetPointer(const CPointerTypeInfo* /*objectType*/,
                                  TObjectPtr objectPtr,
                                  TObjectPtr dataPtr)
{
    CTypeConverter<TObjectPtr>::Get(objectPtr) = dataPtr;
}

TObjectPtr CPointerTypeInfo::CreatePointer(TTypeInfo /*objectType*/,
                                           CObjectMemoryPool* /*memoryPool*/)
{
    return new void*(0);
}

bool CPointerTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return GetObjectPointer(object) == 0;
}

bool CPointerTypeInfo::Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                              ESerialRecursionMode how) const
{
    TConstObjectPtr data1 = GetObjectPointer(object1);
    TConstObjectPtr data2 = GetObjectPointer(object2);
    if ( how != eRecursive ) {
        return how == eShallow ? (data1 == data2) : (data1 == 0 || data2 == 0);
    }
    else if ( data1 == 0 ) {
        return data2 == 0;
    }
    else {
        if ( data2 == 0 )
            return false;
        TTypeInfo type1 = GetRealDataTypeInfo(data1);
        TTypeInfo type2 = GetRealDataTypeInfo(data2);
        return type1 == type2 && type1->Equals(data1, data2, how);
    }
}

void CPointerTypeInfo::SetDefault(TObjectPtr dst) const
{
    SetObjectPointer(dst, 0);
}

void CPointerTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src,
                              ESerialRecursionMode how) const
{
    TConstObjectPtr data = GetObjectPointer(src);
    if ( how != eRecursive ) {
        SetObjectPointer(dst, how == eShallow ? (const_cast<void*>(data)) : 0);
    }
    else if ( data == 0) {
        SetObjectPointer(dst, 0);
    }
    else {
        TTypeInfo type = GetRealDataTypeInfo(data);
        TObjectPtr object = type->Create();
        type->Assign(object, data, how);
        SetObjectPointer(dst, object);
    }
}

void CPointerTypeInfo::ReadPointer(CObjectIStream& in,
                                   TTypeInfo objectType,
                                   TObjectPtr objectPtr)
{
    const CPointerTypeInfo* pointerType =
        CTypeConverter<CPointerTypeInfo>::SafeCast(objectType);

    TTypeInfo pointedType = pointerType->GetPointedType();
    TObjectPtr pointedPtr = pointerType->GetObjectPointer(objectPtr);
    if ( pointedPtr ) {
        //pointedType->SetDefault(pointedPtr);
        in.ReadObject(pointedPtr, pointedType);
    }
    else {
        pointerType->SetObjectPointer(objectPtr,
                                      in.ReadPointer(pointedType).first);
    }
}

void CPointerTypeInfo::WritePointer(CObjectOStream& out,
                                    TTypeInfo objectType,
                                    TConstObjectPtr objectPtr)
{
    const CPointerTypeInfo* pointerType =
        CTypeConverter<CPointerTypeInfo>::SafeCast(objectType);

    out.WritePointer(pointerType->GetObjectPointer(objectPtr),
                     pointerType->GetPointedType());
}

void CPointerTypeInfo::CopyPointer(CObjectStreamCopier& copier,
                                   TTypeInfo objectType)
{
    const CPointerTypeInfo* pointerType =
        CTypeConverter<CPointerTypeInfo>::SafeCast(objectType);

    copier.CopyPointer(pointerType->GetPointedType());
}

void CPointerTypeInfo::SkipPointer(CObjectIStream& in,
                                   TTypeInfo objectType)
{
    const CPointerTypeInfo* pointerType =
        CTypeConverter<CPointerTypeInfo>::SafeCast(objectType);

    in.SkipPointer(pointerType->GetPointedType());
}

END_NCBI_SCOPE
