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
* Revision 1.19  2000/09/01 13:16:21  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.18  2000/07/03 18:42:47  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.17  2000/05/24 20:08:49  vasilche
* Implemented XML dump.
*
* Revision 1.16  2000/04/06 16:11:01  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.15  2000/03/14 14:42:32  vasilche
* Fixed error reporting.
*
* Revision 1.14  2000/03/07 14:06:24  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.13  2000/01/10 19:46:42  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.12  2000/01/05 19:43:57  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.11  1999/12/28 18:55:52  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.10  1999/12/17 19:05:04  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.9  1999/09/23 18:57:02  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.8  1999/09/14 18:54:20  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.7  1999/07/07 21:56:35  vasilche
* Fixed problem with static variables in templates on SunPro
*
* Revision 1.6  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.5  1999/07/07 19:59:08  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.4  1999/06/30 16:05:05  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.3  1999/06/24 14:45:01  vasilche
* Added binary ASN.1 output.
*
* Revision 1.2  1999/06/04 20:51:49  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:57  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <serial/stdtypesimpl.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>

BEGIN_NCBI_SCOPE

CTypeInfo::ETypeFamily CPrimitiveTypeInfo::GetTypeFamily(void) const
{
    return eTypePrimitive;
}

bool CPrimitiveTypeInfo::GetValueBool(TConstObjectPtr /*objectPtr*/) const
{
    ThrowIncompatibleValue();
    return false;
}

void CPrimitiveTypeInfo::SetValueBool(TObjectPtr /*objectPtr*/, bool /*value*/) const
{
    ThrowIncompatibleValue();
}

char CPrimitiveTypeInfo::GetValueChar(TConstObjectPtr /*objectPtr*/) const
{
    ThrowIncompatibleValue();
    return 0;
}

void CPrimitiveTypeInfo::SetValueChar(TObjectPtr /*objectPtr*/, char /*value*/) const
{
    ThrowIncompatibleValue();
}

bool CPrimitiveTypeInfo::IsSigned(void) const
{
    return true;
}

long CPrimitiveTypeInfo::GetValueLong(TConstObjectPtr /*objectPtr*/) const
{
    ThrowIncompatibleValue();
    return 0;
}

void CPrimitiveTypeInfo::SetValueLong(TObjectPtr /*objectPtr*/, long /*value*/) const
{
    ThrowIncompatibleValue();
}

unsigned long CPrimitiveTypeInfo::GetValueULong(TConstObjectPtr /*objectPtr*/) const
{
    ThrowIncompatibleValue();
    return 0;
}

void CPrimitiveTypeInfo::SetValueULong(TObjectPtr /*objectPtr*/,
                                       unsigned long /*value*/) const
{
    ThrowIncompatibleValue();
}

double CPrimitiveTypeInfo::GetValueDouble(TConstObjectPtr /*objectPtr*/) const
{
    ThrowIncompatibleValue();
    return 0;
}

void CPrimitiveTypeInfo::SetValueDouble(TObjectPtr /*objectPtr*/,
                                        double /*value*/) const
{
    ThrowIncompatibleValue();
}

void CPrimitiveTypeInfo::GetValueString(TConstObjectPtr /*objectPtr*/,
                                        string& /*value*/) const
{
    ThrowIncompatibleValue();
}

void CPrimitiveTypeInfo::SetValueString(TObjectPtr /*objectPtr*/,
                                        const string& /*value*/) const
{
    ThrowIncompatibleValue();
}

void CPrimitiveTypeInfo::GetValueOctetString(TConstObjectPtr /*objectPtr*/,
                                             vector<char>& /*value*/) const
{
    ThrowIncompatibleValue();
}

void CPrimitiveTypeInfo::SetValueOctetString(TObjectPtr /*objectPtr*/,
                                             const vector<char>& /*value*/) const
{
    ThrowIncompatibleValue();
}

const CPrimitiveTypeInfo* CPrimitiveTypeInfo::GetIntegerTypeInfo(size_t size)
{
    TTypeInfo info;
    if ( size == sizeof(int) )
        info = CStdTypeInfo<int>::GetTypeInfo();
    else if ( size == sizeof(short) )
        info = CStdTypeInfo<short>::GetTypeInfo();
    else if ( size == sizeof(signed char) )
        info = CStdTypeInfo<signed char>::GetTypeInfo();
    else if ( size == sizeof(long) )
        info = CStdTypeInfo<long>::GetTypeInfo();
    else
        THROW1_TRACE(runtime_error, "Illegal enum size: "+NStr::UIntToString(size));
    _ASSERT(info->GetSize() == size);
    _ASSERT(info->GetTypeFamily() == eTypePrimitive);
    _ASSERT(dynamic_cast<const CPrimitiveTypeInfo*>(info));
    _ASSERT(static_cast<const CPrimitiveTypeInfo*>(info)->GetValueType() == eInteger);
    return static_cast<const CPrimitiveTypeInfo*>(info);
}


TTypeInfo CStdTypeInfo<bool>::GetTypeInfo(void)
{
    static const CPrimitiveTypeInfo* info = new CPrimitiveTypeInfoBool();
    return info;
}

TTypeInfo CStdTypeInfo<char>::GetTypeInfo(void)
{
    static const CPrimitiveTypeInfo* info = new CPrimitiveTypeInfoChar();
    return info;
}

#define SERIAL_ENUMERATE_STD_TYPE(Type, Suffix) \
TTypeInfo CStdTypeInfo<Type>::GetTypeInfo(void) \
{ \
    static const CPrimitiveTypeInfo* info =  new CPrimitiveTypeInfoLong<Type>(); \
    return info; \
}
SERIAL_ENUMERATE_STD_TYPE(signed char,schar)
SERIAL_ENUMERATE_STD_TYPE(unsigned char,uchar)
SERIAL_ENUMERATE_ALL_INTEGRAL_TYPES
#undef SERIAL_ENUMERATE_STD_TYPE

#define SERIAL_ENUMERATE_STD_TYPE(Type, Suffix) \
TTypeInfo CStdTypeInfo<Type>::GetTypeInfo(void) \
{ \
    static const CPrimitiveTypeInfo* info = new CPrimitiveTypeInfoDouble<Type>(); \
    return info; \
}
SERIAL_ENUMERATE_ALL_FLOAT_TYPES
#undef SERIAL_ENUMERATE_STD_TYPE

TTypeInfo CStdTypeInfo<string>::GetTypeInfo(void)
{
    static const CPrimitiveTypeInfo* info = new CPrimitiveTypeInfoString();
    return info;
}

TTypeInfo GetStdTypeInfo_char_ptr(void)
{
    static const CPrimitiveTypeInfo* info =
        new CPrimitiveTypeInfoCharPtr<char*>();
    return info;
}

TTypeInfo GetStdTypeInfo_const_char_ptr(void)
{
    static const CPrimitiveTypeInfo* info =
        new CPrimitiveTypeInfoCharPtr<const char*>();
    return info;
}

CPrimitiveTypeInfo::EValueType CPrimitiveTypeInfoBool::GetValueType(void) const
{
    return eBool;
}

bool CPrimitiveTypeInfoBool::GetValueBool(TConstObjectPtr object) const
{
    return Get(object);
}

void CPrimitiveTypeInfoBool::SetValueBool(TObjectPtr object, bool value) const
{
    Get(object) = value;
}

CPrimitiveTypeInfo::EValueType CPrimitiveTypeInfoChar::GetValueType(void) const
{
    return eChar;
}

char CPrimitiveTypeInfoChar::GetValueChar(TConstObjectPtr object) const
{
    return Get(object);
}

void CPrimitiveTypeInfoChar::SetValueChar(TObjectPtr object, char value) const
{
    Get(object) = value;
}

CPrimitiveTypeInfo::EValueType CVoidTypeInfo::GetValueType(void) const
{
    return eSpecial;
}

size_t CVoidTypeInfo::GetSize(void) const
{
    return 0;
}

bool CVoidTypeInfo::IsDefault(TConstObjectPtr ) const
{
    return true;
}

bool CVoidTypeInfo::Equals(TConstObjectPtr , TConstObjectPtr ) const
{
    ThrowIllegalCall();
    return false;
}

void CVoidTypeInfo::SetDefault(TObjectPtr ) const
{
}

void CVoidTypeInfo::Assign(TObjectPtr , TConstObjectPtr ) const
{
    ThrowIllegalCall();
}

void CVoidTypeInfo::ReadData(CObjectIStream& , TObjectPtr ) const
{
    ThrowIllegalCall();
}

void CVoidTypeInfo::SkipData(CObjectIStream& ) const
{
    ThrowIllegalCall();
}
    
void CVoidTypeInfo::WriteData(CObjectOStream& , TConstObjectPtr ) const
{
    ThrowIllegalCall();
}

void CVoidTypeInfo::CopyData(CObjectStreamCopier& ) const
{
    ThrowIllegalCall();
}

CPrimitiveTypeInfo::EValueType CPrimitiveTypeInfoString::GetValueType(void) const
{
    return eString;
}

size_t CPrimitiveTypeInfoString::GetSize(void) const
{
    return sizeof(TObjectType);
}

TObjectPtr CPrimitiveTypeInfoString::Create(void) const
{
    return new TObjectType();
}

bool CPrimitiveTypeInfoString::IsDefault(TConstObjectPtr object) const
{
    return Get(object).empty();
}

bool CPrimitiveTypeInfoString::Equals(TConstObjectPtr object1,
                                      TConstObjectPtr object2) const
{
    return Get(object1) == Get(object2);
}

void CPrimitiveTypeInfoString::SetDefault(TObjectPtr object) const
{
    Get(object).erase();
}

void CPrimitiveTypeInfoString::Assign(TObjectPtr dst,
                                      TConstObjectPtr src) const
{
    Get(dst) = Get(src);
}

TTypeInfo GetStdTypeInfo_string(void)
{
    static TTypeInfo typeInfo = new CPrimitiveTypeInfoString;
    return typeInfo;
}

void CPrimitiveTypeInfoString::ReadData(CObjectIStream& in,
                                        TObjectPtr object) const
{
	in.ReadStd(Get(object));
}

void CPrimitiveTypeInfoString::SkipData(CObjectIStream& in) const
{
	in.SkipString();
}

void CPrimitiveTypeInfoString::WriteData(CObjectOStream& out,
                                         TConstObjectPtr object) const
{
	out.WriteStd(Get(object));
}

void CPrimitiveTypeInfoString::CopyData(CObjectStreamCopier& copier) const
{
    copier.CopyString();
}

void CPrimitiveTypeInfoString::GetValueString(TConstObjectPtr object,
                                              string& value) const
{
    value = Get(object);
}

void CPrimitiveTypeInfoString::SetValueString(TObjectPtr object,
                                              const string& value) const
{
    Get(object) = value;
}

void CStringStoreTypeInfo::ReadData(CObjectIStream& in,
                                    TObjectPtr object) const
{
	in.ReadStringStore(Get(object));
}

void CStringStoreTypeInfo::SkipData(CObjectIStream& in) const
{
	in.SkipStringStore();
}

void CStringStoreTypeInfo::WriteData(CObjectOStream& out,
                                     TConstObjectPtr object) const
{
	out.WriteStringStore(Get(object));
}

void CStringStoreTypeInfo::CopyData(CObjectStreamCopier& copier) const
{
    copier.CopyStringStore();
}

void CNullBoolTypeInfo::ReadData(CObjectIStream& in,
                                 TObjectPtr object) const
{
	in.ReadNull();
    Get(object) = true;
}

void CNullBoolTypeInfo::SkipData(CObjectIStream& in) const
{
	in.SkipNull();
}

void CNullBoolTypeInfo::WriteData(CObjectOStream& out,
                                  TConstObjectPtr object) const
{
    if ( !Get(object) )
        THROW1_TRACE(runtime_error, "cannot store FALSE as NULL"); 
	out.WriteNull();
}

void CNullBoolTypeInfo::CopyData(CObjectStreamCopier& copier) const
{
    copier.In().ReadNull();
    copier.Out().WriteNull();
}

TTypeInfo CCharVectorTypeInfo<char>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CCharVectorTypeInfoImpl<char>;
    return typeInfo;
}

TTypeInfo CCharVectorTypeInfo<signed char>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CCharVectorTypeInfoImpl<signed char>;
    return typeInfo;
}

TTypeInfo CCharVectorTypeInfo<unsigned char>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CCharVectorTypeInfoImpl<unsigned char>;
    return typeInfo;
}

TTypeInfo GetTypeInfoNullBool(void)
{
    static TTypeInfo typeInfo = new CNullBoolTypeInfo;
    return typeInfo;
}

TTypeInfo GetTypeInfoStringStore(void)
{
    static TTypeInfo typeInfo = new CStringStoreTypeInfo;
    return typeInfo;
}

void ThrowIncompatibleValue(void)
{
    THROW1_TRACE(runtime_error, "incompatible value");
}

void ThrowIllegalCall(void)
{
    THROW1_TRACE(runtime_error, "illegal call");
}

void ThrowIntegerOverflow(void)
{
    THROW1_TRACE(runtime_error, "integer overflow");
}

END_NCBI_SCOPE
