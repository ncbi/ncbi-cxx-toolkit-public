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
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbithr.hpp>
#include <serial/enumvalues.hpp>
#include <serial/impl/enumerated.hpp>
#include <serial/serialutil.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>

BEGIN_NCBI_SCOPE

CEnumeratedTypeValues::CEnumeratedTypeValues(const char* name,
                                             bool isInteger)
    : m_Name(name),
      m_Integer(isInteger),
      m_IsBitset(false), m_IsInternal(false)
{
}

CEnumeratedTypeValues::CEnumeratedTypeValues(const string& name,
                                             bool isInteger)
    : m_Name(name),
      m_Integer(isInteger),
      m_IsBitset(false), m_IsInternal(false)
{
}

CEnumeratedTypeValues::~CEnumeratedTypeValues(void)
{
}

const string& CEnumeratedTypeValues::GetName(void) const
{
    return IsInternal()? kEmptyStr: m_Name;
}

const string& CEnumeratedTypeValues::GetModuleName(void) const
{
    return IsInternal()? kEmptyStr: m_ModuleName;
}

void CEnumeratedTypeValues::SetModuleName(const string& name)
{
    if ( !m_ModuleName.empty() )
        NCBI_THROW(CSerialException,eFail,"cannot change module name: " + m_ModuleName + " to " + name);
    m_ModuleName = name;
}

const string& CEnumeratedTypeValues::GetInternalName(void) const
{
    return !IsInternal()? kEmptyStr: m_Name;
}

const string& CEnumeratedTypeValues::GetInternalModuleName(void) const
{
    return !IsInternal()? kEmptyStr: m_ModuleName;
}

void CEnumeratedTypeValues::SetInternalName(const string& name)
{
    if ( IsInternal() || !m_Name.empty() || !m_ModuleName.empty() )
        NCBI_THROW(CSerialException,eFail, "cannot change (internal) name to " + name);
    m_IsInternal = true;
    m_Name = name;
}

const string& CEnumeratedTypeValues::GetAccessName(void) const
{
    return m_Name;
}

const string& CEnumeratedTypeValues::GetAccessModuleName(void) const
{
    return m_ModuleName;
}

TEnumValueType CEnumeratedTypeValues::FindValue(const CTempString& name) const
{
    const TNameToValue& m = NameToValue();
    TNameToValue::const_iterator i = m.find(name);
    if ( i == m.end() ) {
        string name_alt = string(name);
        name_alt[0] = toupper((unsigned char)name_alt[0]);
        i = m.find(name_alt);
        if ( i == m.end() ) {
            NCBI_THROW(CSerialException,eInvalidData,
                       "invalid value of enumerated type: " + string(name));
        }
    }
    return i->second;
}

bool CEnumeratedTypeValues::IsValidName(const CTempString& name) const
{
    const TNameToValue& m = NameToValue();
    return ( m.find(name) != m.end() );
}

const string& CEnumeratedTypeValues::FindName(TEnumValueType value,
                                              bool allowBadValue) const
{
    const TValueToName& m = ValueToName();
    TValueToName::const_iterator i = m.find(value);
    if ( i == m.end() ) {
        if ( allowBadValue ) {
            return NcbiEmptyString;
        }
        else {
            NCBI_THROW(CSerialException,eInvalidData,
                       "invalid value of enumerated type: " + NStr::NumericToString(value));
        }
    }
    return *i->second;
}

const string& CEnumeratedTypeValues::FindNameEx(TEnumValueType value,
                                                bool allowBadValue) const
{
    if (IsInteger() && (GetValueFlags(value) & eHideIntName) != 0) {
        return kEmptyStr;
    }
    return FindName( value,  allowBadValue);
}

string CEnumeratedTypeValues::GetDisplayName(TEnumValueType value) const
{
    string res;
    if (IsBitset()) {
        TEnumValueType v = value;
        const TValueToName& m = ValueToName();
        TValueToName::const_reverse_iterator i = m.rbegin();
        for (i = m.rbegin(); i != m.rend(); ++i) {
            if ((i->first & v) == i->first) {
                if (!res.empty()) {
                    res.insert(0,",");
                }
                res.insert(0,*(i->second));
                v -= i->first;
            }
        }
        if (v) {
            if (!res.empty()) {
                res += ',';
            }
            res += NStr::NumericToString(v);
        }

    } else {
        res = FindName(value, true);
    }
    if (res.empty()) {
        res = NStr::NumericToString(value);
    }
    return res;
}

void CEnumeratedTypeValues::AddValue(const string& name,
    TEnumValueType value, TValueFlags flags)
{
    if ( name.empty() ) {
        NCBI_THROW(CSerialException,eInvalidData,
                   "empty enum value name");
    }
    m_Values.push_back(make_pair(name, value));
    m_ValueFlags[value] = flags;
    m_ValueToName.reset(0);
    m_NameToValue.reset(0);
}


CEnumeratedTypeValues::TValueFlags
CEnumeratedTypeValues::GetValueFlags(TEnumValueType value) const
{
    map<TEnumValueType, TValueFlags>::const_iterator i = m_ValueFlags.find(value);
    return i != m_ValueFlags.end() ? i->second : eNone;
}

DEFINE_STATIC_FAST_MUTEX(s_EnumValuesMutex);

const CEnumeratedTypeValues::TValueToName&
CEnumeratedTypeValues::ValueToName(void) const
{
    TValueToName* m = m_ValueToName.get();
    if ( !m ) {
        CFastMutexGuard GUARD(s_EnumValuesMutex);
        m = m_ValueToName.get();
        if ( !m ) {
            auto_ptr<TValueToName> keep(m = new TValueToName);
            ITERATE ( TValues, i, m_Values ) {
                (*m)[i->second] = &i->first;
            }
            m_ValueToName = keep;
        }
    }
    return *m;
}

const CEnumeratedTypeValues::TNameToValue&
CEnumeratedTypeValues::NameToValue(void) const
{
    TNameToValue* m = m_NameToValue.get();
    if ( !m ) {
        CFastMutexGuard GUARD(s_EnumValuesMutex);
        m = m_NameToValue.get();
        if ( !m ) {
            auto_ptr<TNameToValue> keep(m = new TNameToValue);
            ITERATE ( TValues, i, m_Values ) {
                const string& s = i->first;
                pair<TNameToValue::iterator, bool> p =
                    m->insert(TNameToValue::value_type(s, i->second));
                if ( !p.second ) {
                    NCBI_THROW(CSerialException,eInvalidData,
                               "duplicate enum value name: " + s);
                }
            }
            m_NameToValue = keep;
        }
    }
    return *m;
}

void CEnumeratedTypeValues::AddValue(const char* name,
    TEnumValueType value, TValueFlags flags)
{
    AddValue(string(name), value, flags);
}

CEnumeratedTypeInfo::CEnumeratedTypeInfo(size_t size,
                                         const CEnumeratedTypeValues* values,
                                         bool sign)
    : CParent(size, values->GetName(), ePrimitiveValueEnum, sign),
      m_ValueType(CPrimitiveTypeInfo::GetIntegerTypeInfo(size, sign)),
      m_Values(*values)
{
    SetTag(CAsnBinaryDefs::eEnumerated);
    _ASSERT(m_ValueType->GetPrimitiveValueType() == ePrimitiveValueInteger);
    if ( values->IsInternal() )
        SetInternalName(values->GetInternalName());
    const string& module_name = values->GetAccessModuleName();
    if ( !module_name.empty() )
        SetModuleName(module_name);
    SetCreateFunction(&CreateEnum);
    SetReadFunction(&ReadEnum);
    SetWriteFunction(&WriteEnum);
    SetCopyFunction(&CopyEnum);
    SetSkipFunction(&SkipEnum);
}

bool CEnumeratedTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return m_ValueType->IsDefault(object);
}

bool CEnumeratedTypeInfo::Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                                 ESerialRecursionMode how) const
{
    return m_ValueType->Equals(object1, object2, how);
}

void CEnumeratedTypeInfo::SetDefault(TObjectPtr dst) const
{
    m_ValueType->SetDefault(dst);
}

void CEnumeratedTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src,
                                 ESerialRecursionMode how) const
{
    m_ValueType->Assign(dst, src, how);
}

bool CEnumeratedTypeInfo::IsSigned(void) const
{
    return m_ValueType->IsSigned();
}

Int4 CEnumeratedTypeInfo::GetValueInt4(TConstObjectPtr objectPtr) const
{
    return m_ValueType->GetValueInt4(objectPtr);
}

Uint4 CEnumeratedTypeInfo::GetValueUint4(TConstObjectPtr objectPtr) const
{
    return m_ValueType->GetValueUint4(objectPtr);
}

void CEnumeratedTypeInfo::SetValueInt4(TObjectPtr objectPtr, Int4 value) const
{
    if ( !Values().IsInteger() ) {
        // check value for acceptance
        _ASSERT(sizeof(TEnumValueType) == sizeof(value));
        TEnumValueType v = TEnumValueType(value);
        Values().FindName(v, false);
    }
    m_ValueType->SetValueInt4(objectPtr, value);
}

void CEnumeratedTypeInfo::SetValueUint4(TObjectPtr objectPtr,
                                        Uint4 value) const
{
    if ( !Values().IsInteger() ) {
        // check value for acceptance
        _ASSERT(sizeof(TEnumValueType) == sizeof(value));
        TEnumValueType v = TEnumValueType(value);
        if ( v < 0 ) {
            NCBI_THROW(CSerialException,eOverflow,"overflow error");
        }
        Values().FindName(v, false);
    }
    m_ValueType->SetValueUint4(objectPtr, value);
}

Int8 CEnumeratedTypeInfo::GetValueInt8(TConstObjectPtr objectPtr) const
{
    return m_ValueType->GetValueInt8(objectPtr);
}

Uint8 CEnumeratedTypeInfo::GetValueUint8(TConstObjectPtr objectPtr) const
{
    return m_ValueType->GetValueUint8(objectPtr);
}

void CEnumeratedTypeInfo::SetValueInt8(TObjectPtr objectPtr, Int8 value) const
{
    if ( !Values().IsInteger() ) {
        // check value for acceptance
        _ASSERT(sizeof(TEnumValueType) < sizeof(value));
        TEnumValueType v = TEnumValueType(value);
        if ( v != value )
            NCBI_THROW(CSerialException,eOverflow,"overflow error");
        Values().FindName(v, false);
    }
    m_ValueType->SetValueInt8(objectPtr, value);
}

void CEnumeratedTypeInfo::SetValueUint8(TObjectPtr objectPtr,
                                        Uint8 value) const
{
    if ( !Values().IsInteger() ) {
        // check value for acceptance
        _ASSERT(sizeof(TEnumValueType) < sizeof(value));
        TEnumValueType v = TEnumValueType(value);
        if ( v < 0 || Uint8(v) != value )
            NCBI_THROW(CSerialException,eOverflow,"overflow error");
        Values().FindName(v, false);
    }
    m_ValueType->SetValueUint8(objectPtr, value);
}

void CEnumeratedTypeInfo::GetValueString(TConstObjectPtr objectPtr,
                                         string& value) const
{
    value = Values().FindName(m_ValueType->GetValueInt(objectPtr), false);
}

void CEnumeratedTypeInfo::SetValueString(TObjectPtr objectPtr,
                                         const string& value) const
{
    m_ValueType->SetValueInt(objectPtr, Values().FindValue(value));
}

TObjectPtr CEnumeratedTypeInfo::CreateEnum(TTypeInfo objectType,
                                           CObjectMemoryPool* /*memoryPool*/)
{
    const CEnumeratedTypeInfo* enumType =
        CTypeConverter<CEnumeratedTypeInfo>::SafeCast(objectType);
    return enumType->m_ValueType->Create();
}

void CEnumeratedTypeInfo::ReadEnum(CObjectIStream& in,
                                   TTypeInfo objectType,
                                   TObjectPtr objectPtr)
{
    const CEnumeratedTypeInfo* enumType =
        CTypeConverter<CEnumeratedTypeInfo>::SafeCast(objectType);
    try {
        enumType->m_ValueType->SetValueInt(objectPtr,
                                           in.ReadEnum(enumType->Values()));
    }
    catch ( CException& e ) {
        NCBI_RETHROW_SAME(e,"invalid enum value");
    }
    catch ( ... ) {
        in.ThrowError(in.fInvalidData,"invalid enum value");
    }
}

void CEnumeratedTypeInfo::WriteEnum(CObjectOStream& out,
                                    TTypeInfo objectType,
                                    TConstObjectPtr objectPtr)
{
    const CEnumeratedTypeInfo* enumType =
        CTypeConverter<CEnumeratedTypeInfo>::SafeCast(objectType);
    try {
        out.WriteEnum(enumType->Values(),
                      enumType->m_ValueType->GetValueInt(objectPtr));
    }
    catch ( CException& e ) {
        NCBI_RETHROW_SAME(e,"invalid enum value");
    }
    catch ( ... ) {
        out.ThrowError(out.fInvalidData,"invalid enum value");
    }
}

void CEnumeratedTypeInfo::CopyEnum(CObjectStreamCopier& copier,
                                   TTypeInfo objectType)
{
    const CEnumeratedTypeInfo* enumType =
        CTypeConverter<CEnumeratedTypeInfo>::SafeCast(objectType);
    try {
        copier.Out().CopyEnum(enumType->Values(), copier.In());
    }
    catch ( CException& e ) {
        NCBI_RETHROW_SAME(e,"invalid enum value");
    }
    catch ( ... ) {
        copier.ThrowError(CObjectIStream::fInvalidData,"invalid enum value");
    }
}

void CEnumeratedTypeInfo::SkipEnum(CObjectIStream& in,
                                   TTypeInfo objectType)
{
    const CEnumeratedTypeInfo* enumType =
        CTypeConverter<CEnumeratedTypeInfo>::SafeCast(objectType);
    try {
        in.ReadEnum(enumType->Values());
    }
    catch ( CException& e ) {
        NCBI_RETHROW_SAME(e,"invalid enum value");
    }
    catch ( ... ) {
        in.ThrowError(in.fInvalidData,"invalid enum value");
    }
}

END_NCBI_SCOPE
