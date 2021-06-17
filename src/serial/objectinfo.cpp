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
#include <serial/impl/objstrasnb.hpp>
#include <regex>

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
    return GetClassTypeInfo()->GetMembers().Find(tag, CAsnBinaryDefs::eContextSpecific);
}

TMemberIndex CObjectTypeInfo::FindVariantIndex(const string& name) const
{
    return GetChoiceTypeInfo()->GetVariants().Find(name);
}

TMemberIndex CObjectTypeInfo::FindVariantIndex(int tag) const
{
    return GetChoiceTypeInfo()->GetVariants().Find(tag, CAsnBinaryDefs::eContextSpecific);
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

CObjectTypeInfo::TASNTag CObjectTypeInfo::GetASNTag() const
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
    vector<int>& pattern, size_t& pos, int depth,
    const CItemInfo* item) const
{
    bool good = false;
    TASNTag tag = GetASNTag();
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
                    .Find(pattern[pos+1], CAsnBinaryDefs::eContextSpecific);
                good = i != kInvalidMember &&
                    GetMemberIterator(i).GetMemberType()
                        .MatchPattern(pattern,pos,depth,
                            GetMemberIterator(i).GetItemInfo());
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
                .Find(pattern[pos+1], CAsnBinaryDefs::eContextSpecific);
            good = i != kInvalidMember &&
                GetVariantIterator(i).GetVariantType()
                    .MatchPattern(pattern,pos,depth,
                        GetVariantIterator(i).GetItemInfo());
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
                        .Find(pattern[pos+1], CAsnBinaryDefs::eContextSpecific);
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
            if (item && item->NonEmpty()) {
                good = elem_count != 0;
            } else {
                good = true;
            }
        }
        break;
    case eTypeFamilyPointer:
        good = GetPointedType().MatchPattern(pattern, pos, depth, item);
        break;
    default:
        break;
    }
    return good;
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


CSerialFacet::CSerialFacet(void) {
}
CSerialFacet::~CSerialFacet(void) {
}

void CSerialFacet::Validate(TTypeInfo info, TConstObjectPtr object, const CObjectStack& stk) const {
    Validate( CConstObjectInfo(object, info), stk);
}

class CSerialFacetImpl : public CSerialFacet
{
public:
    CSerialFacetImpl(ESerialFacet type) : m_Next(nullptr), m_Type(type) {
    }
    virtual ~CSerialFacetImpl(void) {
        if (m_Next) {
            delete m_Next;
        }
    }
    virtual void Validate( const CConstObjectInfo& oi, const CObjectStack& stk) const override {
        if (m_Next) {
            m_Next->Validate(oi, stk);
        }
    }
protected:
    void ValidateContainerElements( const CConstObjectInfo& oi, const CObjectStack& stk) const {
        CConstObjectInfo::CElementIterator b = oi.BeginElements();
        for ( ; b.Valid(); ++b) {
            Validate(b.GetElement(), stk);
        }
    }
    static string GetLocation(const CObjectStack& stk) {
        return string("Restriction check failed at ") + stk.GetPosition() + " (" + stk.GetStackTrace() + "): ";
    }

protected:
    const CSerialFacet* m_Next;
    ESerialFacet m_Type;
    friend class CItemInfo;
};

class CSerialFacetPattern : public CSerialFacetImpl
{
public:
    CSerialFacetPattern(ESerialFacet type, const string& value) : CSerialFacetImpl(type), m_Value(value) {
    }
    virtual void Validate( const CConstObjectInfo& oi, const CObjectStack& stk) const override {
        if ( oi.GetTypeFamily() == eTypeFamilyPrimitive && oi.GetPrimitiveValueType() == ePrimitiveValueString) {
            string v;
            oi.GetPrimitiveValueString(v);
            if (!regex_match(v, regex(m_Value))) {
                NCBI_THROW(CSerialFacetException,ePattern, GetLocation(stk) +
                    "value \"" + v + "\", does not match pattern \"" + m_Value + "\"");
            }
        }
        else if ( oi.GetTypeFamily() == eTypeFamilyContainer) {
            CSerialFacetPattern(m_Type, m_Value).ValidateContainerElements( oi, stk);
        }
        CSerialFacetImpl::Validate( oi, stk);
    }
    void AddPattern(const string& value) {
        m_Value += "|" + value;
    }
private:
    string m_Value;
};

class CSerialFacetLength : public CSerialFacetImpl
{
public:
    CSerialFacetLength(ESerialFacet type, Uint8 value) : CSerialFacetImpl(type), m_Value(value) {
    }
    virtual void Validate( const CConstObjectInfo& oi, const CObjectStack& stk) const override {
        if (oi.GetTypeFamily() == eTypeFamilyPrimitive) {
            Uint8 v;
            switch (oi.GetPrimitiveValueType()) {
            case ePrimitiveValueString:
                {
                    string t;
                    oi.GetPrimitiveValueString(t);
                    v = t.size();
                }
                break;
            case ePrimitiveValueOctetString:
                {
                    vector<char> t;
                    oi.GetPrimitiveValueOctetString(t);
                    v = t.size();
                }
                break;
            case ePrimitiveValueBitString:
                {
                    CBitString t;
                    oi.GetPrimitiveValueBitString(t);
                    v = t.size();
                }
                break;
            default:
                CSerialFacetImpl::Validate(oi, stk);
                return;
            }
            switch (m_Type) {
            case ESerialFacet::eMinLength:
                if (v < m_Value) {
                    NCBI_THROW(CSerialFacetException,eMinLength, GetLocation(stk) +
                        "string is too short (" + NStr::NumericToString(v) + "), must have MinLength = " + NStr::NumericToString(m_Value));
                }
                break;
            case ESerialFacet::eMaxLength:
                if (v > m_Value) {
                    NCBI_THROW(CSerialFacetException,eMaxLength, GetLocation(stk) +
                        "string is too long (" + NStr::NumericToString(v) + "), must have MaxLength = " + NStr::NumericToString(m_Value));
                }
                break;
            case ESerialFacet::eLength:
                if (v != m_Value) {
                    NCBI_THROW(CSerialFacetException,eLength, GetLocation(stk) +
                        "string has invalid length (" + NStr::NumericToString(v) + "), must have Length = " + NStr::NumericToString(m_Value));
                }
                break;
            default:
                break;
            }
        }
        else if ( oi.GetTypeFamily() == eTypeFamilyContainer) {
            CSerialFacetLength(m_Type, m_Value).ValidateContainerElements(oi, stk);
        }
        CSerialFacetImpl::Validate(oi, stk);
    }
private:
    Uint8 m_Value;
};

template<typename T> 
typename std::enable_if< std::is_same<T, Int8>::value, T>::type
xxx_Value(const CConstObjectInfo& oi)
{
    return oi.GetPrimitiveValueInt8();
}
template<typename T> 
typename std::enable_if< std::is_same<T, Uint8>::value, T>::type
xxx_Value(const CConstObjectInfo& oi)
{
    return oi.GetPrimitiveValueUint8();
}
template<typename T> 
typename std::enable_if< std::is_same<T, double>::value, T>::type
xxx_Value(const CConstObjectInfo& oi)
{
    return oi.GetPrimitiveValueDouble();
}

template<typename TValue>
class CSerialFacetMultipleOf : public CSerialFacetImpl
{
public:
    CSerialFacetMultipleOf(ESerialFacet type, TValue value) : CSerialFacetImpl(type), m_Value(value) {
    }
    virtual void Validate(const CConstObjectInfo& oi, const CObjectStack& stk) const override {
        if ( oi.GetTypeFamily() == eTypeFamilyPrimitive && oi.GetPrimitiveValueType() == ePrimitiveValueInteger) {
            TValue v = xxx_Value<TValue>(oi);
            if (v % m_Value != 0) {
                NCBI_THROW(CSerialFacetException,eMultipleOf, GetLocation(stk) +
                    "invalid value (" + NStr::NumericToString(v) + "), must be MultipleOf " + NStr::NumericToString(m_Value));
            }
        }
        else if ( oi.GetTypeFamily() == eTypeFamilyContainer) {
            CSerialFacetMultipleOf<TValue>(m_Type, m_Value).ValidateContainerElements(oi, stk);
        }
        CSerialFacetImpl::Validate(oi, stk);
    }
private:
    TValue m_Value;
};

template<typename TValue>
class CSerialFacetValue : public CSerialFacetImpl
{
public:
    CSerialFacetValue(ESerialFacet type, TValue value) : CSerialFacetImpl(type), m_Value(value) {
    }
    virtual void Validate( const CConstObjectInfo& oi, const CObjectStack& stk) const override {
        if ( oi.GetTypeFamily() == eTypeFamilyPrimitive && 
            (oi.GetPrimitiveValueType() == ePrimitiveValueInteger || oi.GetPrimitiveValueType() == ePrimitiveValueReal)) {
            TValue v = xxx_Value<TValue>(oi);
            switch (m_Type) {
            case ESerialFacet::eInclusiveMinimum:
                if (v < m_Value) {
                    NCBI_THROW(CSerialFacetException,eInclusiveMinimum, GetLocation(stk) +
                        "invalid value (" + NStr::NumericToString(v) + "), eInclusiveMinimum = " + NStr::NumericToString(m_Value));
                }
                break;
            case ESerialFacet::eExclusiveMinimum:
                if (v <= m_Value) {
                    NCBI_THROW(CSerialFacetException,eExclusiveMinimum, GetLocation(stk) +
                        "invalid value (" + NStr::NumericToString(v) + "), ExclusiveMinimum = " + NStr::NumericToString(m_Value));
                }
                break;
            case ESerialFacet::eInclusiveMaximum:
                if (v > m_Value) {
                    NCBI_THROW(CSerialFacetException,eInclusiveMaximum, GetLocation(stk) +
                        "invalid value (" + NStr::NumericToString(v) + "), eInclusiveMaximum = " + NStr::NumericToString(m_Value));
                }
                break;
            case ESerialFacet::eExclusiveMaximum:
                if (v >= m_Value) {
                    NCBI_THROW(CSerialFacetException,eExclusiveMaximum, GetLocation(stk) +
                        "invalid value (" + NStr::NumericToString(v) + "), ExclusiveMaximum = " + NStr::NumericToString(m_Value));
                }
                break;
            default:
                break;
            }
        }
        else if ( oi.GetTypeFamily() == eTypeFamilyContainer) {
            CSerialFacetValue<TValue>(m_Type, m_Value).ValidateContainerElements(oi, stk);
        }
        CSerialFacetImpl::Validate(oi, stk);
    }
private:
    TValue m_Value;
};

class CSerialFacetContainer : public CSerialFacetImpl
{
public:
    CSerialFacetContainer(ESerialFacet type, Uint8 value) : CSerialFacetImpl(type), m_Value(value) {
    }
    virtual void Validate(const CConstObjectInfo& oi, const CObjectStack& stk) const override {
        if ( oi.GetTypeFamily() == eTypeFamilyContainer) {
            CConstObjectInfo::CElementIterator b = oi.BeginElements();
            Uint8 v = b.GetElementCount();
            switch (m_Type) {
            case ESerialFacet::eMinItems:
                if (v < m_Value) {
                    NCBI_THROW(CSerialFacetException,eMinItems, GetLocation(stk) +
                        "array is too short (" + NStr::NumericToString(v) + "), must have MinItems = " + NStr::NumericToString(m_Value));
                }
                break;
            case ESerialFacet::eMaxItems:
                if (v > m_Value) {
                    NCBI_THROW(CSerialFacetException,eMaxItems, GetLocation(stk) +
                        "array is too long (" + NStr::NumericToString(v) + "), must have MaxItems = " + NStr::NumericToString(m_Value));
                }
                break;
            case ESerialFacet::eUniqueItems:
                if (m_Value) {
                    size_t ib=0, ie=0;
                    CConstObjectInfo::CElementIterator e = b;
                    for ( ; b.Valid(); ++b, ++ib) {
                        for ( e=b, ie=ib+1, ++e; e.Valid(); ++e, ++ie) {
                            if (b.GetElement().GetTypeInfo()->Equals( b.GetElement().GetObjectPtr(), e.GetElement().GetObjectPtr())) {
                                NCBI_THROW(CSerialFacetException,eUniqueItems, GetLocation(stk) +
                                    "array contains identical elements: #" + NStr::NumericToString(ib) + " and #" + NStr::NumericToString(ie) + ", must have UniqueItems");
                            }
                        }
                    }
                }
                break;
            default:
                break;
            }
        }
        CSerialFacetImpl::Validate(oi, stk);
    }
private:
    Uint8 m_Value;
};

CItemInfo* CItemInfo::Restrict( ESerialFacet type, const string& pattern)
{
    CSerialFacetImpl* next = nullptr;
    if (type == ESerialFacet::ePattern) {
        for (next = (CSerialFacetImpl*)m_Restrict; next; next = (CSerialFacetImpl*)next->m_Next) {
            if (next->m_Type == ESerialFacet::ePattern) {
                ((CSerialFacetPattern*)next)->AddPattern(pattern);
                return this;
            }
        }
        next = new CSerialFacetPattern(type, pattern);
    }
    if (next) {
        next->m_Next = m_Restrict;
        m_Restrict = next;
    }
    return this;
}

CItemInfo* CItemInfo::Restrict(ESerialFacet type, Uint8 value)
{
    CSerialFacetImpl* next = nullptr;
    switch (type) {
    case ESerialFacet::eMinLength:
    case ESerialFacet::eMaxLength: 
    case ESerialFacet::eLength: 
        next = new CSerialFacetLength(type,value); break;
    case ESerialFacet::eMultipleOf:
        next = new CSerialFacetMultipleOf<Uint8>(type,value); break;
    case ESerialFacet::eInclusiveMinimum:
    case ESerialFacet::eExclusiveMinimum:
    case ESerialFacet::eInclusiveMaximum:
    case ESerialFacet::eExclusiveMaximum:
        next = new CSerialFacetValue<Uint8>(type,value); break;
    case ESerialFacet::eMinItems:
    case ESerialFacet::eMaxItems:
    case ESerialFacet::eUniqueItems:
        next = new CSerialFacetContainer(type,value); break;
    default: break;
    }
    if (next) {
        next->m_Next = m_Restrict;
        m_Restrict = next;
    }
    return this;
}

CItemInfo* CItemInfo::RestrictI(ESerialFacet type, Int8 value)
{
    CSerialFacetImpl* next = nullptr;
    switch (type) {
    case ESerialFacet::eMultipleOf:
        next = new CSerialFacetMultipleOf<Int8>(type,value); break;
    case ESerialFacet::eInclusiveMinimum:
    case ESerialFacet::eExclusiveMinimum:
    case ESerialFacet::eInclusiveMaximum:
    case ESerialFacet::eExclusiveMaximum:
        next = new CSerialFacetValue<Int8>(type,value); break;
    default: break;
    }
    if (next) {
        next->m_Next = m_Restrict;
        m_Restrict = next;
    }
    return this;
}
CItemInfo* CItemInfo::RestrictD(ESerialFacet type, double value)
{
    CSerialFacetImpl* next = nullptr;
    switch (type) {
    case ESerialFacet::eInclusiveMinimum:
    case ESerialFacet::eExclusiveMinimum:
    case ESerialFacet::eInclusiveMaximum:
    case ESerialFacet::eExclusiveMaximum:
        next = new CSerialFacetValue<double>(type,value); break;
    default: break;
    }
    if (next) {
        next->m_Next = m_Restrict;
        m_Restrict = next;
    }
    return this;
}

END_NCBI_SCOPE
