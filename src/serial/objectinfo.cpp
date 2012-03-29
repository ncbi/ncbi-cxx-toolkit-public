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
#include <corelib/ncbistd.hpp>
#include <serial/exception.hpp>
#include <serial/objectinfo.hpp>
#include <serial/impl/stdtypesimpl.hpp>

BEGIN_NCBI_SCOPE

// object type info

void CObjectTypeInfo::WrongTypeFamily(ETypeFamily /*needFamily*/) const
{
    NCBI_THROW(CSerialException,eInvalidData, "wrong type family");
}

const CPrimitiveTypeInfo* CObjectTypeInfo::GetPrimitiveTypeInfo(void) const
{
    CheckTypeFamily(eTypeFamilyPrimitive);
    return CTypeConverter<CPrimitiveTypeInfo>::SafeCast(GetTypeInfo());
}

const CEnumeratedTypeInfo* CObjectTypeInfo::GetEnumeratedTypeInfo(void) const
{
    CheckTypeFamily(eTypeFamilyPrimitive);
    return CTypeConverter<CEnumeratedTypeInfo>::SafeCast(GetTypeInfo());
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
CObjectTypeInfo CObjectTypeInfo::GetPointedType(void) const
{
    return GetPointerTypeInfo()->GetPointedType();
}

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

const CEnumeratedTypeValues& CObjectTypeInfo::GetEnumeratedTypeValues(void) const
{
    return GetEnumeratedTypeInfo()->Values();
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

int CConstObjectInfo::GetPrimitiveValueInt(void) const
{
    return GetPrimitiveTypeInfo()->GetValueInt(GetObjectPtr());
}

unsigned CConstObjectInfo::GetPrimitiveValueUInt(void) const
{
    return GetPrimitiveTypeInfo()->GetValueUInt(GetObjectPtr());
}

long CConstObjectInfo::GetPrimitiveValueLong(void) const
{
    return GetPrimitiveTypeInfo()->GetValueLong(GetObjectPtr());
}

unsigned long CConstObjectInfo::GetPrimitiveValueULong(void) const
{
    return GetPrimitiveTypeInfo()->GetValueULong(GetObjectPtr());
}

Int4 CConstObjectInfo::GetPrimitiveValueInt4(void) const
{
    return GetPrimitiveTypeInfo()->GetValueInt4(GetObjectPtr());
}

Uint4 CConstObjectInfo::GetPrimitiveValueUint4(void) const
{
    return GetPrimitiveTypeInfo()->GetValueUint4(GetObjectPtr());
}

Int8 CConstObjectInfo::GetPrimitiveValueInt8(void) const
{
    return GetPrimitiveTypeInfo()->GetValueInt8(GetObjectPtr());
}

Uint8 CConstObjectInfo::GetPrimitiveValueUint8(void) const
{
    return GetPrimitiveTypeInfo()->GetValueUint8(GetObjectPtr());
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

void CConstObjectInfo::GetPrimitiveValueBitString(CBitString& value) const
{
    GetPrimitiveTypeInfo()->GetValueBitString(GetObjectPtr(), value);
}

void CConstObjectInfo::GetPrimitiveValueAnyContent(CAnyContentObject& value)
    const
{
    GetPrimitiveTypeInfo()->GetValueAnyContent(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueBool(bool value)
{
    GetPrimitiveTypeInfo()->SetValueBool(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueChar(char value)
{
    GetPrimitiveTypeInfo()->SetValueChar(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueInt4(Int4 value)
{
    GetPrimitiveTypeInfo()->SetValueInt4(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueUint4(Uint4 value)
{
    GetPrimitiveTypeInfo()->SetValueUint4(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueInt8(Int8 value)
{
    GetPrimitiveTypeInfo()->SetValueInt8(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueUint8(Uint8 value)
{
    GetPrimitiveTypeInfo()->SetValueUint8(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueInt(int value)
{
    GetPrimitiveTypeInfo()->SetValueInt(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueUInt(unsigned int value)
{
    GetPrimitiveTypeInfo()->SetValueUInt(GetObjectPtr(), value);
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

void CObjectInfo::SetPrimitiveValueBitString(const CBitString& value)
{
    GetPrimitiveTypeInfo()->SetValueBitString(GetObjectPtr(), value);
}

void CObjectInfo::SetPrimitiveValueAnyContent(const CAnyContentObject& value)
{
    GetPrimitiveTypeInfo()->SetValueAnyContent(GetObjectPtr(), value);
}

TMemberIndex CConstObjectInfo::GetCurrentChoiceVariantIndex(void) const
{
    return GetChoiceTypeInfo()->GetIndex(GetObjectPtr());
}

CObjectInfo CObjectInfo::SetPointedObject(void) const
{
    const CPointerTypeInfo* pointerType = GetPointerTypeInfo();
    TObjectPtr pointerPtr = GetObjectPtr();
    TTypeInfo pointeeType = pointerType->GetPointedType();
    TObjectPtr pointeePtr = pointerType->GetObjectPointer(pointerPtr);
    if ( !pointeePtr ) {
        pointeePtr = pointeeType->Create();
        pointerType->SetObjectPointer(pointerPtr, pointeePtr);
    }
    return CObjectInfo(pointeePtr, pointeeType);
}


CObjectInfo CObjectInfo::AddNewElement(void) const
{
    const CContainerTypeInfo* container_type = GetContainerTypeInfo();
    return CObjectInfo(container_type->AddElement(GetObjectPtr(), 0),
                       container_type->GetElementType());
}


CObjectInfo CObjectInfo::AddNewPointedElement(void) const
{
    const CContainerTypeInfo* container_type = GetContainerTypeInfo();
    TTypeInfo element_type = container_type->GetElementType();
    if ( element_type->GetTypeFamily() != eTypeFamilyPointer )
        WrongTypeFamily(eTypeFamilyPointer);
    const CPointerTypeInfo* pointer_type =
        CTypeConverter<CPointerTypeInfo>::SafeCast(element_type);
    TTypeInfo pointee_type = pointer_type->GetPointedType();
    TObjectPtr pointee_ptr = pointee_type->Create();
    CObjectInfo ret(pointee_ptr, pointee_type);
    container_type->AddElement(GetObjectPtr(), &pointee_ptr, eShallow);
    return ret;
}


CObjectInfo CObjectInfo::SetClassMember(TMemberIndex index) const
{
    const CClassTypeInfo* class_type = GetClassTypeInfo();
    TObjectPtr class_ptr = GetObjectPtr();
    const CMemberInfo* member_info = class_type->GetMemberInfo(index);
    member_info->UpdateSetFlagMaybe(class_ptr);
    return CObjectInfo(member_info->GetMemberPtr(class_ptr),
                       member_info->GetTypeInfo());
}

CObjectInfo CObjectInfo::SetChoiceVariant(TMemberIndex index) const
{
    const CChoiceTypeInfo* choice_type = GetChoiceTypeInfo();
    TObjectPtr choice_ptr = GetObjectPtr();
    choice_type->SetIndex(choice_ptr, index);
    _ASSERT(choice_type->GetIndex(choice_ptr) == index);
    const CVariantInfo* variant_info = choice_type->GetVariantInfo(index);
    return CObjectInfo(variant_info->GetVariantPtr(choice_ptr),
                       variant_info->GetTypeInfo());
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

void CObjectTypeInfo::SetPathReadHook(CObjectIStream* stream, const string& path,
                         CReadObjectHook* hook) const
{
    GetNCTypeInfo()->SetPathReadHook(stream,path,hook);
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

void CObjectTypeInfo::SetPathWriteHook(CObjectOStream* stream, const string& path,
                          CWriteObjectHook* hook) const
{
    GetNCTypeInfo()->SetPathWriteHook(stream,path,hook);
}

void CObjectTypeInfo::SetLocalSkipHook(CObjectIStream& stream,
                                       CSkipObjectHook* hook) const
{
    GetNCTypeInfo()->SetLocalSkipHook(stream, hook);
}

void CObjectTypeInfo::ResetLocalSkipHook(CObjectIStream& stream) const
{
    GetNCTypeInfo()->ResetLocalSkipHook(stream);
}

void CObjectTypeInfo::SetPathSkipHook(CObjectIStream* stream, const string& path,
                         CSkipObjectHook* hook) const
{
    GetNCTypeInfo()->SetPathSkipHook(stream,path,hook);
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

void CObjectTypeInfo::SetPathCopyHook(CObjectStreamCopier* stream, const string& path,
                         CCopyObjectHook* hook) const
{
    GetNCTypeInfo()->SetPathCopyHook(stream,path,hook);
}

CAsnBinaryDefs::ETagValue CObjectTypeInfo::GetASNTag() const
{
    switch (GetTypeFamily()) {
    default:
        break;
    case eTypeFamilyPrimitive:

        switch (GetPrimitiveValueType()) {
        case ePrimitiveValueSpecial:     return CAsnBinaryDefs::eNull;
        case ePrimitiveValueBool:        return CAsnBinaryDefs::eBoolean;
        case ePrimitiveValueChar:        return CAsnBinaryDefs::eGeneralString;
        case ePrimitiveValueInteger:     return CAsnBinaryDefs::eInteger;
        case ePrimitiveValueReal:        return CAsnBinaryDefs::eReal;
        case ePrimitiveValueOctetString: return CAsnBinaryDefs::eOctetString;
        case ePrimitiveValueBitString:   return CAsnBinaryDefs::eBitString;
        case ePrimitiveValueAny:         return CAsnBinaryDefs::eNone;
        case ePrimitiveValueOther:       return CAsnBinaryDefs::eNone;
        case ePrimitiveValueString:
        {
            const CPrimitiveTypeInfoString *p =
                CTypeConverter<CPrimitiveTypeInfoString>::SafeCast(
                    GetTypeInfo());
            if (p->GetStringType() ==
                    CPrimitiveTypeInfoString::eStringTypeUTF8) {
                return CAsnBinaryDefs::eUTF8String;
            }
            if (p->IsStringStore()) {
               return CAsnBinaryDefs::eStringStore;
            }
            return CAsnBinaryDefs::eVisibleString;
        }
        case ePrimitiveValueEnum:
            if (GetEnumeratedTypeValues().IsInteger()) {
                return CAsnBinaryDefs::eInteger;
            }
            return CAsnBinaryDefs::eEnumerated;
        default:
            break;
        }
        break;
    case eTypeFamilyClass:
        if (GetClassTypeInfo()->Implicit()) {
            break;
        }
        if (GetClassTypeInfo()->RandomOrder()) {
            return CAsnBinaryDefs::eSet;
        }
        return CAsnBinaryDefs::eSequence;
    case eTypeFamilyChoice: return CAsnBinaryDefs::eSequence;
    case eTypeFamilyContainer:
        if (GetContainerTypeInfo()->RandomElementsOrder()) {
            return CAsnBinaryDefs::eSet;
        }
        return CAsnBinaryDefs::eSequence;
    case eTypeFamilyPointer:
        return CAsnBinaryDefs::eNone;
    }
    return CAsnBinaryDefs::eNone;
}

bool CObjectTypeInfo::MatchPattern(
    vector<int>& pattern, size_t& pos, int depth) const
{
    bool good = false;
    CAsnBinaryDefs::ETagValue tag = GetASNTag();
    if (tag != CAsnBinaryDefs::eNone) {
        good = pattern[pos] == depth && pattern[pos+2] == (int)tag;
        if (!good) {
            if (tag == CAsnBinaryDefs::eSequence &&
                GetTypeFamily() == eTypeFamilyChoice) {
                depth -= 1;
                good = true;
            }
            if ((tag == CAsnBinaryDefs::eSequence || tag == CAsnBinaryDefs::eSet) &&
                pattern[pos] == depth && 
                pattern[pos+2] == (int)CAsnBinaryDefs::eNull) {
                pos += 3;
                return true;
            }
            // patch for already fixed bug in the toolkit
            if (tag == CAsnBinaryDefs::eUTF8String) {
                good = pattern[pos] == depth &&
                       pattern[pos+2] == (int)CAsnBinaryDefs::eVisibleString;
            }
            if (!good) {
                return good;
            }
        }
        pos += 3;
        if (pos+2 >= pattern.size()) {
            return good;
        }
    }

    switch (GetTypeFamily()) {
    case eTypeFamilyClass:
        if (GetClassTypeInfo()->Implicit()) {
            if (pattern[pos] == depth) {
                size_t prev = pos;
                good = BeginMembers().GetMemberType().MatchPattern(pattern,pos,depth);
                if (!good) {
                    pos = prev;
                }
            }
        } else {
            depth+=2;
            while (pattern[pos] == depth) {
                size_t prev = pos;
                TMemberIndex i = GetClassTypeInfo()->GetItems()
                    .Find(pattern[pos+1]);
                good = i != kInvalidMember &&
                    GetMemberIterator(i).GetMemberType()
                        .MatchPattern(pattern,pos,depth);
                if (!good) {
                    pos = prev;
                    break;
                }
                if (pos+2 >= pattern.size()) {
                    break;
                }
            }
        }
        break;
    case eTypeFamilyChoice:
        depth+=2;
        {
            size_t prev = pos;
            TMemberIndex i = GetChoiceTypeInfo()->GetItems()
                .Find(pattern[pos+1]);
            good = i != kInvalidMember &&
                GetVariantIterator(i).GetVariantType()
                    .MatchPattern(pattern,pos,depth);
            if (!good) {
                pos = prev;
            }
        }
        break;
    case eTypeFamilyContainer:
        {
            CObjectTypeInfo e(GetElementType());
            do {
                while (e.GetTypeFamily() == eTypeFamilyPointer) {
                    e = e.GetPointedType();
                }
                if (e.GetTypeFamily() == eTypeFamilyClass &&
                    e.GetClassTypeInfo()->Implicit()) {
                    e = e.BeginMembers().GetMemberType();
                }
            } while (e.GetTypeFamily() == eTypeFamilyPointer);
            size_t elem_count = 0;
            if (e.GetTypeFamily() == eTypeFamilyChoice) {
                depth+=2;
                for (;;) {
                    size_t prev = pos;
                    TMemberIndex i = e.GetChoiceTypeInfo()->GetItems()
                        .Find(pattern[pos+1]);
                    good = i != kInvalidMember &&
                        e.GetVariantIterator(i).GetVariantType()
                            .MatchPattern(pattern,pos,depth);
                    if (good) {
                        ++elem_count;
                    }
                    if (pos+2 >= pattern.size()) {
                        break;
                    }
                    if (!good) {
                        pos = prev;
                        break;
                    }
                }
            } else {
                depth += 1;
                for (;;) {
                    size_t prev = pos;
                    good = GetElementType().MatchPattern(pattern,pos,depth);
                    if (good) {
                        ++elem_count;
                    }
                    if (pos+2 >= pattern.size()) {
                        break;
                    }
                    if (!good) {
                        pos = prev;
                        break;
                    }
                }
            }
            good = elem_count != 0;
        }
        break;
    case eTypeFamilyPointer:
        good = GetPointedType().MatchPattern(pattern, pos, depth);
        break;
    default:
        break;
    }
    return good;
}

END_NCBI_SCOPE
