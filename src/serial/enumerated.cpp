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
* Revision 1.30  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.29  2004/05/17 21:03:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.28  2004/03/25 15:57:08  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.27  2003/05/16 18:02:18  gouriano
* revised exception error messages
*
* Revision 1.26  2003/03/11 20:08:07  kuznets
* iterate -> ITERATE
*
* Revision 1.25  2003/03/10 18:54:25  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.24  2002/10/25 14:49:27  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.23  2002/09/19 20:05:44  vasilche
* Safe initialization of static mutexes
*
* Revision 1.22  2002/09/05 21:21:32  vasilche
* Added mutex for enum values map
*
* Revision 1.21  2002/08/30 16:21:32  vasilche
* Added MT lock for cache maps
*
* Revision 1.20  2001/05/17 15:07:06  lavr
* Typos corrected
*
* Revision 1.19  2001/01/30 21:41:04  vasilche
* Fixed dealing with unsigned enums.
*
* Revision 1.18  2000/12/15 15:38:42  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.17  2000/11/07 17:25:40  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.16  2000/10/20 15:51:38  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.15  2000/10/17 18:45:33  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.14  2000/09/26 18:09:48  vasilche
* Fixed some warnings.
*
* Revision 1.13  2000/09/18 20:00:21  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.12  2000/09/01 13:16:15  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.11  2000/08/15 19:44:47  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.10  2000/07/03 18:42:43  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.9  2000/06/07 19:45:58  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.8  2000/06/01 19:07:02  vasilche
* Added parsing of XML data.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbithr.hpp>
#include <serial/enumvalues.hpp>
#include <serial/enumerated.hpp>
#include <serial/serialutil.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>

BEGIN_NCBI_SCOPE

CEnumeratedTypeValues::CEnumeratedTypeValues(const char* name,
                                             bool isInteger)
    : m_Name(name), m_Integer(isInteger)
{
}

CEnumeratedTypeValues::CEnumeratedTypeValues(const string& name,
                                             bool isInteger)
    : m_Name(name), m_Integer(isInteger)
{
}

CEnumeratedTypeValues::~CEnumeratedTypeValues(void)
{
}

void CEnumeratedTypeValues::SetModuleName(const string& name)
{
    if ( !m_ModuleName.empty() )
        NCBI_THROW(CSerialException,eFail,"cannot change module name");
    m_ModuleName = name;
}

TEnumValueType CEnumeratedTypeValues::FindValue(const CLightString& name) const
{
    const TNameToValue& m = NameToValue();
    TNameToValue::const_iterator i = m.find(name);
    if ( i == m.end() ) {
        NCBI_THROW(CSerialException,eInvalidData,
                   "invalid value of enumerated type");
    }
    return i->second;
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
                       "invalid value of enumerated type");
        }
    }
    return *i->second;
}

void CEnumeratedTypeValues::AddValue(const string& name, TEnumValueType value)
{
    if ( name.empty() ) {
        NCBI_THROW(CSerialException,eInvalidData,
                   "empty enum value name");
    }
    m_Values.push_back(make_pair(name, value));
    m_ValueToName.reset(0);
    m_NameToValue.reset(0);
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
                               "duplicate enum value name");
                }
            }
            m_NameToValue = keep;
        }
    }
    return *m;
}

void CEnumeratedTypeValues::AddValue(const char* name, TEnumValueType value)
{
    AddValue(string(name), value);
}

CEnumeratedTypeInfo::CEnumeratedTypeInfo(size_t size,
                                         const CEnumeratedTypeValues* values,
                                         bool sign)
    : CParent(size, values->GetName(), ePrimitiveValueEnum, sign),
      m_ValueType(CPrimitiveTypeInfo::GetIntegerTypeInfo(size, sign)),
      m_Values(*values)
{
    _ASSERT(m_ValueType->GetPrimitiveValueType() == ePrimitiveValueInteger);
    if ( !values->GetModuleName().empty() )
        SetModuleName(values->GetModuleName());
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
        in.ThrowError(in.fFormatError,"invalid enum value");
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
        copier.ThrowError(CObjectIStream::fFormatError,"invalid enum value");
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
        in.ThrowError(in.fFormatError,"invalid enum value");
    }
}

END_NCBI_SCOPE
