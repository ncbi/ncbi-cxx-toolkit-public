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
* Revision 1.24  2000/10/20 15:51:43  vasilche
* Fixed data error processing.
* Added interface for costructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.23  2000/10/17 18:45:36  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.22  2000/10/13 20:22:56  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.21  2000/10/13 16:28:40  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.20  2000/09/18 20:00:25  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
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

#include <serial/serialdef.hpp>
#include <serial/stdtypesimpl.hpp>
#include <serial/typeinfoimpl.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>

#include <limits.h>
#if HAVE_WINDOWS_H
// In MSVC limits.h doesn't define FLT_MIN & FLT_MAX
# include <float.h>
#endif

BEGIN_NCBI_SCOPE

template<typename T>
class CPrimitiveTypeFunctions
{
public:
    typedef T TObjectType;

    static TObjectType& Get(TObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }

    static void SetIOFunctions(CPrimitiveTypeInfo* info)
        {
            info->SetIOFunctions(&Read, &Write, &Copy, &Skip);
        }

    static void SetMemFunctions(CPrimitiveTypeInfo* info)
        {
            info->SetMemFunctions(&Create,
                                  &IsDefault, &SetDefault,
                                  &Equals, &Assign);
        }

    static TObjectPtr Create(TTypeInfo )
        {
            return new TObjectType(0);
        }
    static bool IsDefault(TConstObjectPtr objectPtr)
        {
            return Get(objectPtr) == TObjectType(0);
        }
    static void SetDefault(TObjectPtr objectPtr)
        {
            Get(objectPtr) = TObjectType(0);
        }

    static bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2)
        {
            return Get(obj1) == Get(obj2);
        }
    static void Assign(TObjectPtr dst, TConstObjectPtr src)
        {
            Get(dst) = Get(src);
        }

    static void Read(CObjectIStream& in,
                     TTypeInfo , TObjectPtr objectPtr)
        {
            in.ReadStd(Get(objectPtr));
        }
    static void Write(CObjectOStream& out,
                      TTypeInfo , TConstObjectPtr objectPtr)
        {
            out.WriteStd(Get(objectPtr));
        }
    static void Skip(CObjectIStream& in, TTypeInfo )
        {
            TObjectType data;
            in.ReadStd(data);
        }
    static void Copy(CObjectStreamCopier& copier, TTypeInfo )
        {
            TObjectType data;
            copier.In().ReadStd(data);
            copier.Out().WriteStd(data);
        }
};

void CVoidTypeFunctions::ThrowException(const char* operation,
                                        TTypeInfo objectType)
{
    string message("cannot ");
    message += operation;
    message += " object of type: ";
    message += objectType->GetName();
    THROW1_TRACE(runtime_error, message);
}

bool CVoidTypeFunctions::IsDefault(TConstObjectPtr )
{
    return true;
}

bool CVoidTypeFunctions::Equals(TConstObjectPtr , TConstObjectPtr )
{
    ThrowIllegalCall();
    return false;
}

void CVoidTypeFunctions::SetDefault(TObjectPtr )
{
}

void CVoidTypeFunctions::Assign(TObjectPtr , TConstObjectPtr )
{
    ThrowIllegalCall();
}

void CVoidTypeFunctions::Read(CObjectIStream& in, TTypeInfo ,
                              TObjectPtr )
{
    in.ThrowError(in.eIllegalCall, "cannot read");
}

void CVoidTypeFunctions::Write(CObjectOStream& out, TTypeInfo ,
                               TConstObjectPtr )
{
    out.ThrowError(out.eIllegalCall, "cannot write");
}

void CVoidTypeFunctions::Copy(CObjectStreamCopier& copier, TTypeInfo )
{
    copier.ThrowError(CObjectIStream::eIllegalCall, "cannot copy");
}

void CVoidTypeFunctions::Skip(CObjectIStream& in, TTypeInfo )
{
    in.ThrowError(in.eIllegalCall, "cannot skip");
}

TObjectPtr CVoidTypeFunctions::Create(TTypeInfo objectType)
{
    ThrowException("create", objectType);
    return 0;
}

CVoidTypeInfo::CVoidTypeInfo(void)
    : CParent(0, ePrimitiveValueSpecial)
{
}

CPrimitiveTypeInfo::CPrimitiveTypeInfo(size_t size,
                                       EPrimitiveValueType valueType,
                                       bool isSigned)
    : CParent(eTypeFamilyPrimitive, size),
      m_ValueType(valueType), m_Signed(isSigned)
{
    typedef CVoidTypeFunctions TFunctions;
    SetMemFunctions(&TFunctions::Create,
                    &TFunctions::IsDefault, &TFunctions::SetDefault,
                    &TFunctions::Equals, &TFunctions::Assign);
}

CPrimitiveTypeInfo::CPrimitiveTypeInfo(size_t size, const char* name,
                                       EPrimitiveValueType valueType,
                                       bool isSigned)
    : CParent(eTypeFamilyPrimitive, size, name),
      m_ValueType(valueType), m_Signed(isSigned)
{
    typedef CVoidTypeFunctions TFunctions;
    SetMemFunctions(&TFunctions::Create,
                    &TFunctions::IsDefault, &TFunctions::SetDefault,
                    &TFunctions::Equals, &TFunctions::Assign);
}

CPrimitiveTypeInfo::CPrimitiveTypeInfo(size_t size, const string& name,
                                       EPrimitiveValueType valueType,
                                       bool isSigned)
    : CParent(eTypeFamilyPrimitive, size, name),
      m_ValueType(valueType), m_Signed(isSigned)
{
    typedef CVoidTypeFunctions TFunctions;
    SetMemFunctions(&TFunctions::Create,
                    &TFunctions::IsDefault, &TFunctions::SetDefault,
                    &TFunctions::Equals, &TFunctions::Assign);
}

void CPrimitiveTypeInfo::SetMemFunctions(TTypeCreate create,
                                         TIsDefaultFunction isDefault,
                                         TSetDefaultFunction setDefault,
                                         TEqualsFunction equals,
                                         TAssignFunction assign)
{
    SetCreateFunction(create);
    m_IsDefault = isDefault;
    m_SetDefault = setDefault;
    m_Equals = equals;
    m_Assign = assign;
}

void CPrimitiveTypeInfo::SetIOFunctions(TTypeReadFunction read,
                                        TTypeWriteFunction write,
                                        TTypeCopyFunction copy,
                                        TTypeSkipFunction skip)
{
    SetReadFunction(read);
    SetWriteFunction(write);
    SetCopyFunction(copy);
    SetSkipFunction(skip);
}

bool CPrimitiveTypeInfo::GetValueBool(TConstObjectPtr /*objectPtr*/) const
{
    ThrowIncompatibleValue();
    return false;
}

bool CPrimitiveTypeInfo::IsDefault(TConstObjectPtr objectPtr) const
{
    return m_IsDefault(objectPtr);
}

void CPrimitiveTypeInfo::SetDefault(TObjectPtr objectPtr) const
{
    m_SetDefault(objectPtr);
}

bool CPrimitiveTypeInfo::Equals(TConstObjectPtr obj1,
                                    TConstObjectPtr obj2) const
{
    return m_Equals(obj1, obj2);
}

void CPrimitiveTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    m_Assign(dst, src);
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


CPrimitiveTypeInfoBool::CPrimitiveTypeInfoBool(void)
    : CParent(sizeof(TObjectType), ePrimitiveValueBool)
{
    CPrimitiveTypeFunctions<TObjectType>::SetMemFunctions(this);
    CPrimitiveTypeFunctions<TObjectType>::SetIOFunctions(this);
}

bool CPrimitiveTypeInfoBool::GetValueBool(TConstObjectPtr object) const
{
    return CPrimitiveTypeFunctions<TObjectType>::Get(object);
}

void CPrimitiveTypeInfoBool::SetValueBool(TObjectPtr object, bool value) const
{
    CPrimitiveTypeFunctions<TObjectType>::Get(object) = value;
}

TTypeInfo CStdTypeInfo<bool>::GetTypeInfo(void)
{
    static const CPrimitiveTypeInfo* info = new CPrimitiveTypeInfoBool();
    return info;
}

class CNullBoolFunctions : public CPrimitiveTypeFunctions<bool>
{
public:
    static void Read(CObjectIStream& in, TTypeInfo ,
                     TObjectPtr object)
        {
            in.ReadNull();
            Get(object) = true;
        }
    static void Write(CObjectOStream& out, TTypeInfo ,
                      TConstObjectPtr object)
        {
            if ( !Get(object) )
                THROW1_TRACE(runtime_error, "cannot store FALSE as NULL"); 
            out.WriteNull();
        }
    static void Copy(CObjectStreamCopier& copier, TTypeInfo )
        {
            copier.In().ReadNull();
            copier.Out().WriteNull();
        }
    static void Skip(CObjectIStream& in, TTypeInfo )
        {
            in.SkipNull();
        }
};

TTypeInfo CStdTypeInfo<bool>::GetTypeInfoNullBool(void)
{
    static CPrimitiveTypeInfo* info = 0;
    if ( !info ) {
        info = new CPrimitiveTypeInfoBool;
        typedef CNullBoolFunctions TFunctions;
        info->SetIOFunctions(&TFunctions::Read, &TFunctions::Write,
                             &TFunctions::Copy, &TFunctions::Skip);
    }
    return info;
}

CPrimitiveTypeInfoChar::CPrimitiveTypeInfoChar(void)
    : CParent(sizeof(TObjectType), ePrimitiveValueChar)
{
    CPrimitiveTypeFunctions<TObjectType>::SetMemFunctions(this);
    CPrimitiveTypeFunctions<TObjectType>::SetIOFunctions(this);
}

char CPrimitiveTypeInfoChar::GetValueChar(TConstObjectPtr object) const
{
    return CPrimitiveTypeFunctions<TObjectType>::Get(object);
}

void CPrimitiveTypeInfoChar::SetValueChar(TObjectPtr object, char value) const
{
    CPrimitiveTypeFunctions<TObjectType>::Get(object) = value;
}

void CPrimitiveTypeInfoChar::GetValueString(TConstObjectPtr object,
                                            string& value) const
{
    value.assign(1, CPrimitiveTypeFunctions<TObjectType>::Get(object));
}

void CPrimitiveTypeInfoChar::SetValueString(TObjectPtr object,
                                            const string& value) const
{
    if ( value.size() != 1 )
        ThrowIncompatibleValue();
    CPrimitiveTypeFunctions<TObjectType>::Get(object) = value[0];
}

TTypeInfo CStdTypeInfo<char>::GetTypeInfo(void)
{
    static const CPrimitiveTypeInfo* info = new CPrimitiveTypeInfoChar();
    return info;
}

template<typename T>
class CPrimitiveTypeInfoLongFunctions : public CPrimitiveTypeFunctions<T>
{
public:
    typedef T TObjectType;
    
    static const CPrimitiveTypeInfoLong* CreateTypeInfo(void)
        {
            CPrimitiveTypeInfoLong* info =
                new CPrimitiveTypeInfoLong(sizeof(TObjectType), x_IsSigned());

            info->SetMemFunctions(&Create,
                                  &IsDefault, &SetDefault, &Equals, &Assign);

            info->SetIOFunctions(&Read, &Write, &Copy, &Skip);

            info->SetLongFunctions(&GetValueLong, &SetValueLong,
                                   &GetValueULong, &SetValueULong);

            return info;
        }

    static bool IsDefault(TConstObjectPtr objectPtr)
        {
            return Get(objectPtr) == 0;
        }
    static void SetDefault(TObjectPtr objectPtr)
        {
            Get(objectPtr) = 0;
        }
    static bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2)
        {
            return Get(obj1) == Get(obj2);
        }
    static void Assign(TObjectPtr dst, TConstObjectPtr src)
        {
            Get(dst) = Get(src);
        }

    static bool x_IsSigned(void)
        {
            return TObjectType(-1) < 0;
        }
    static void x_CheckNoSign(long value)
        {
            if ( value < 0 ) // value doesn't fit
                ThrowIntegerOverflow();
        }

    static long GetValueLong(TConstObjectPtr objectPtr)
        {
            TObjectType value = Get(objectPtr);
            if ( sizeof(TObjectType) == sizeof(long) && !x_IsSigned() ) {
                // type is unsigned long
                x_CheckNoSign(long(value));
            }
            return long(value);
        }
    static unsigned long GetValueULong(TConstObjectPtr objectPtr)
        {
            TObjectType value = Get(objectPtr);
            if ( sizeof(TObjectType) == sizeof(long) && x_IsSigned() ) {
                // type is signed long
                x_CheckNoSign(long(value));
            }
            return static_cast<unsigned long>(value);
        }
    static void SetValueLong(TObjectPtr objectPtr, long value)
        {
            TObjectType newValue = TObjectType(value);
            if ( sizeof(TObjectType) == sizeof(long) ) {
                if ( !x_IsSigned() ) {
                    // type is unsigned long
                    x_CheckNoSign(value);
                }
            }
            else {
                // type is smaller than long
                if ( long(newValue) != value )
                    ThrowIntegerOverflow();
            }
            Get(objectPtr) = newValue;
        }
    static void SetValueULong(TObjectPtr objectPtr, unsigned long value)
        {
            TObjectType newValue = TObjectType(value);
            if ( sizeof(TObjectType) == sizeof(long) ) {
                if ( x_IsSigned() ) {
                    // type is signed long
                    x_CheckNoSign(long(newValue));
                }
            }
            else {
                // type is smaller than long
                if ( static_cast<unsigned long>(newValue) != value )
                    ThrowIntegerOverflow();
            }
            Get(objectPtr) = newValue;
        }
};

CPrimitiveTypeInfoLong::CPrimitiveTypeInfoLong(size_t size, bool isSigned)
    : CParent(size, ePrimitiveValueInteger, isSigned)
{
}

void CPrimitiveTypeInfoLong::SetLongFunctions(TGetLongFunction getLong,
                                              TSetLongFunction setLong,
                                              TGetULongFunction getULong,
                                              TSetULongFunction setULong)
{
    m_GetLong = getLong;
    m_SetLong = setLong;
    m_GetULong = getULong;
    m_SetULong = setULong;
}

long CPrimitiveTypeInfoLong::GetValueLong(TConstObjectPtr objectPtr) const
{
    return m_GetLong(objectPtr);
}

unsigned long CPrimitiveTypeInfoLong::GetValueULong(TConstObjectPtr objectPtr) const
{
    return m_GetULong(objectPtr);
}

void CPrimitiveTypeInfoLong::SetValueLong(TObjectPtr objectPtr,
                                          long value) const
{
    m_SetLong(objectPtr, value);
}

void CPrimitiveTypeInfoLong::SetValueULong(TObjectPtr objectPtr,
                                           unsigned long value) const
{
    m_SetULong(objectPtr, value);
}

#define DECLARE_STD_LONG_TYPE(Type) \
TTypeInfo CStdTypeInfo<Type>::GetTypeInfo(void) \
{ \
    static const CPrimitiveTypeInfo* info = \
        CPrimitiveTypeInfoLongFunctions<Type>::CreateTypeInfo(); \
    return info; \
}
DECLARE_STD_LONG_TYPE(signed char)
DECLARE_STD_LONG_TYPE(unsigned char)
DECLARE_STD_LONG_TYPE(short)
DECLARE_STD_LONG_TYPE(unsigned short)
DECLARE_STD_LONG_TYPE(int)
DECLARE_STD_LONG_TYPE(unsigned)
DECLARE_STD_LONG_TYPE(long)
DECLARE_STD_LONG_TYPE(unsigned long)
#if HAVE_LONG_LONG
DECLARE_STD_LONG_TYPE(long long)
DECLARE_STD_LONG_TYPE(unsigned long long)
#endif

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
    else {
        string message("Illegal enum size: ");
        message += NStr::UIntToString(size);
        THROW1_TRACE(runtime_error, message);
    }
    _ASSERT(info->GetSize() == size);
    _ASSERT(info->GetTypeFamily() == eTypeFamilyPrimitive);
    _ASSERT(CTypeConverter<CPrimitiveTypeInfo>::SafeCast(info)->GetPrimitiveValueType() == ePrimitiveValueInteger);
    return CTypeConverter<CPrimitiveTypeInfo>::SafeCast(info);
}

CPrimitiveTypeInfoDouble::CPrimitiveTypeInfoDouble(void)
    : CParent(sizeof(TObjectType), ePrimitiveValueReal, true)
{
    CPrimitiveTypeFunctions<TObjectType>::SetMemFunctions(this);
    CPrimitiveTypeFunctions<TObjectType>::SetIOFunctions(this);
}

double CPrimitiveTypeInfoDouble::GetValueDouble(TConstObjectPtr objectPtr) const
{
    return CPrimitiveTypeFunctions<TObjectType>::Get(objectPtr);
}

void CPrimitiveTypeInfoDouble::SetValueDouble(TObjectPtr objectPtr, double value) const
{
    CPrimitiveTypeFunctions<TObjectType>::Get(objectPtr) = value;
}

TTypeInfo CStdTypeInfo<double>::GetTypeInfo(void)
{
    static const CPrimitiveTypeInfo* info = new CPrimitiveTypeInfoDouble();
    return info;
}

CPrimitiveTypeInfoFloat::CPrimitiveTypeInfoFloat(void)
    : CParent(sizeof(TObjectType), ePrimitiveValueReal, true)
{
    CPrimitiveTypeFunctions<TObjectType>::SetMemFunctions(this);
    CPrimitiveTypeFunctions<TObjectType>::SetIOFunctions(this);
}

double CPrimitiveTypeInfoFloat::GetValueDouble(TConstObjectPtr objectPtr) const
{
    return CPrimitiveTypeFunctions<TObjectType>::Get(objectPtr);
}

void CPrimitiveTypeInfoFloat::SetValueDouble(TObjectPtr objectPtr, double value) const
{
#if defined(FLT_MIN) && defined(FLT_MAX)
    if ( value < FLT_MIN || value > FLT_MAX )
        ThrowIncompatibleValue();
#endif
    CPrimitiveTypeFunctions<TObjectType>::Get(objectPtr) = TObjectType(value);
}

TTypeInfo CStdTypeInfo<float>::GetTypeInfo(void)
{
    static const CPrimitiveTypeInfo* info = new CPrimitiveTypeInfoFloat();
    return info;
}

class CStringFunctions : public CPrimitiveTypeFunctions<string>
{
public:
    static TObjectPtr Create(TTypeInfo )
        {
            return new TObjectType();
        }
    static bool IsDefault(TConstObjectPtr objectPtr)
        {
            return Get(objectPtr).empty();
        }
    static void SetDefault(TObjectPtr objectPtr)
        {
            Get(objectPtr).erase();
        }
    static void Copy(CObjectStreamCopier& copier, TTypeInfo )
        {
            copier.CopyString();
        }
    static void Skip(CObjectIStream& in, TTypeInfo )
        {
            in.SkipString();
        }
};

CPrimitiveTypeInfoString::CPrimitiveTypeInfoString(void)
    : CParent(sizeof(string), ePrimitiveValueString)
{
    typedef CStringFunctions TFunctions;
    SetMemFunctions(&TFunctions::Create,
                    &TFunctions::IsDefault, &TFunctions::SetDefault,
                    &TFunctions::Equals, &TFunctions::Assign);
    SetIOFunctions(&TFunctions::Read, &TFunctions::Write,
                   &TFunctions::Copy, &TFunctions::Skip);
}

void CPrimitiveTypeInfoString::GetValueString(TConstObjectPtr object,
                                              string& value) const
{
    value = CPrimitiveTypeFunctions<TObjectType>::Get(object);
}

void CPrimitiveTypeInfoString::SetValueString(TObjectPtr object,
                                              const string& value) const
{
    CPrimitiveTypeFunctions<TObjectType>::Get(object) = value;
}

char CPrimitiveTypeInfoString::GetValueChar(TConstObjectPtr object) const
{
    if ( CPrimitiveTypeFunctions<TObjectType>::Get(object).size() != 1 )
        ThrowIncompatibleValue();
    return CPrimitiveTypeFunctions<TObjectType>::Get(object)[0];
}

void CPrimitiveTypeInfoString::SetValueChar(TObjectPtr object,
                                            char value) const
{
    CPrimitiveTypeFunctions<TObjectType>::Get(object).assign(1, value);
}

TTypeInfo CStdTypeInfo<string>::GetTypeInfo(void)
{
    static const CPrimitiveTypeInfo* info = new CPrimitiveTypeInfoString();
    return info;
}

class CStringStoreFunctions : public CStringFunctions
{
public:
    static void Read(CObjectIStream& in, TTypeInfo , TObjectPtr objectPtr)
        {
            in.ReadStringStore(Get(objectPtr));
        }
    static void Write(CObjectOStream& out, TTypeInfo ,
                      TConstObjectPtr objectPtr)
        {
            out.WriteStringStore(Get(objectPtr));
        }
    static void Copy(CObjectStreamCopier& copier, TTypeInfo )
        {
            copier.CopyStringStore();
        }
    static void Skip(CObjectIStream& in, TTypeInfo )
        {
            in.SkipStringStore();
        }
};

TTypeInfo CStdTypeInfo<string>::GetTypeInfoStringStore(void)
{
    static CPrimitiveTypeInfo* info = 0;
    if ( !info ) {
        info = new CPrimitiveTypeInfoString;
        typedef CStringStoreFunctions TFunctions;
        info->SetIOFunctions(&TFunctions::Read, &TFunctions::Write,
                             &TFunctions::Copy, &TFunctions::Skip);
    }
    return info;
}

template<typename T>
class CCharPtrFunctions : public CPrimitiveTypeFunctions<T>
{
public:
    static bool IsDefault(TConstObjectPtr objectPtr)
        {
            return Get(objectPtr) == 0;
        }
    static void SetDefault(TObjectPtr dst)
        {
            free(const_cast<char*>(Get(dst)));
            Get(dst) = 0;
        }
    static bool Equals(TConstObjectPtr object1, TConstObjectPtr object2)
        {
            return strcmp(Get(object1), Get(object2)) == 0;
        }
    static void Assign(TObjectPtr dst, TConstObjectPtr src)
        {
            TObjectType value = Get(src);
            _ASSERT(Get(dst) != value);
            free(const_cast<char*>(Get(dst)));
            if ( value )
                Get(dst) = NotNull(strdup(value));
            else
                Get(dst) = 0;
        }
};

template<typename T>
CPrimitiveTypeInfoCharPtr<T>::CPrimitiveTypeInfoCharPtr(void)
    : CParent(sizeof(TObjectType), ePrimitiveValueString)
{
    typedef CCharPtrFunctions<TObjectType> TFunctions;
    SetMemFunctions(&CVoidTypeFunctions::Create,
                    &TFunctions::IsDefault, &TFunctions::SetDefault,
                    &TFunctions::Equals, &TFunctions::Assign);
    SetIOFunctions(&TFunctions::Read, &TFunctions::Write,
                   &TFunctions::Copy, &TFunctions::Skip);
}

template<typename T>
char CPrimitiveTypeInfoCharPtr<T>::GetValueChar(TConstObjectPtr objectPtr) const
{
    TObjectType obj = CCharPtrFunctions<TObjectType>::Get(objectPtr);
    if ( !obj || obj[0] == '\0' || obj[1] != '\0' )
        ThrowIncompatibleValue();
    return obj[0];
}

template<typename T>
void CPrimitiveTypeInfoCharPtr<T>::SetValueChar(TObjectPtr objectPtr,
                                                char value) const
{
    char* obj = static_cast<char*>(NotNull(malloc(2)));
    obj[0] =  value;
    obj[1] = '\0';
    CCharPtrFunctions<TObjectPtr>::Get(objectPtr) = obj;
}

template<typename T>
void CPrimitiveTypeInfoCharPtr<T>::GetValueString(TConstObjectPtr objectPtr,
                                                  string& value) const
{
    value = CCharPtrFunctions<TObjectType>::Get(objectPtr);
}

template<typename T>
void CPrimitiveTypeInfoCharPtr<T>::SetValueString(TObjectPtr objectPtr,
                                                  const string& value) const
{
    CCharPtrFunctions<TObjectPtr>::Get(objectPtr) =
        NotNull(strdup(value.c_str()));
}

TTypeInfo CStdTypeInfo<char*>::GetTypeInfo(void)
{
    static const CPrimitiveTypeInfo* info =
        new CPrimitiveTypeInfoCharPtr<char*>();
    return info;
}

TTypeInfo CStdTypeInfo<const char*>::GetTypeInfo(void)
{
    static const CPrimitiveTypeInfo* info =
        new CPrimitiveTypeInfoCharPtr<const char*>();
    return info;
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

class CCharVectorFunctionsBase
{
public:
    static void Copy(CObjectStreamCopier& copier,
                     TTypeInfo )
        {
            copier.CopyByteBlock();
        }
    static void Skip(CObjectIStream& in, TTypeInfo )
        {
            in.SkipByteBlock();
        }
};

template<typename Char>
class CCharVectorFunctions : public CCharVectorFunctionsBase
{
public:
    typedef vector<Char> TObjectType;
    typedef Char TChar;

    static TObjectType& Get(TObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }

    static char* ToChar(TChar* p)
        { return reinterpret_cast<char*>(p); }
    static const char* ToChar(const TChar* p)
        { return reinterpret_cast<const char*>(p); }
    static const TChar* ToTChar(const char* p)
        { return reinterpret_cast<const TChar*>(p); }

    static TObjectPtr Create(TTypeInfo )
        {
            return new TObjectType();
        }
    static bool IsDefault(TConstObjectPtr object)
        {
            return Get(object).empty();
        }
    static bool Equals(TConstObjectPtr object1, TConstObjectPtr object2)
        {
            return Get(object1) == Get(object2);
        }
    static void SetDefault(TObjectPtr dst)
        {
            Get(dst).clear();
        }
    static void Assign(TObjectPtr dst, TConstObjectPtr src)
        {
            Get(dst) = Get(src);
        }

    static void Read(CObjectIStream& in,
                     TTypeInfo , TObjectPtr objectPtr)
        {
            TObjectType& o = Get(objectPtr);
            CObjectIStream::ByteBlock block(in);
            if ( block.KnownLength() ) {
                size_t length = block.GetExpectedLength();
                o.resize(length);
                block.Read(ToChar(&o.front()), length, true);
            }
            else {
                // length is unknown -> copy via buffer
                Char buffer[4096];
                size_t count;
                o.clear();
                while ( (count = block.Read(ToChar(buffer),
                                            sizeof(buffer))) != 0 ) {
                    o.insert(o.end(), buffer, buffer + count);
                }
            }
            block.End();
        }
    static void Write(CObjectOStream& out,
                      TTypeInfo , TConstObjectPtr objectPtr)
        {
            const TObjectType& o = Get(objectPtr);
            size_t length = o.size();
            CObjectOStream::ByteBlock block(out, length);
            if ( length > 0 )
                block.Write(ToChar(&o.front()), length);
            block.End();
        }
};

template<typename Char>
CCharVectorTypeInfo<Char>::CCharVectorTypeInfo(void)
    : CParent(sizeof(TObjectType), ePrimitiveValueOctetString)
{
    typedef CCharVectorFunctions<Char> TFunctions;
    SetMemFunctions(&TFunctions::Create,
                    &TFunctions::IsDefault, &TFunctions::SetDefault,
                    &TFunctions::Equals, &TFunctions::Assign);
    SetIOFunctions(&TFunctions::Read, &TFunctions::Write,
                   &TFunctions::Copy, &TFunctions::Skip);
}

template<typename Char>
void CCharVectorTypeInfo<Char>::GetValueString(TConstObjectPtr objectPtr,
                                               string& value) const
{
    const TObjectType& obj = CCharVectorFunctions<TChar>::Get(objectPtr);
    const char* buffer = CCharVectorFunctions<TChar>::ToChar(&obj.front());
    value.assign(buffer, buffer + obj.size());
}

template<typename Char>
void CCharVectorTypeInfo<Char>::SetValueString(TObjectPtr objectPtr,
                                               const string& value) const
{
    TObjectType& obj = CCharVectorFunctions<TChar>::Get(objectPtr);
    obj.clear();
    const TChar* buffer = CCharVectorFunctions<TChar>::ToTChar(value.data());
    obj.insert(obj.end(), buffer, buffer + value.size());
}

template<typename Char>
void CCharVectorTypeInfo<Char>::GetValueOctetString(TConstObjectPtr objectPtr,
                                                    vector<char>& value) const
{
    const TObjectType& obj = CCharVectorFunctions<TChar>::Get(objectPtr);
    value.clear();
    const char* buffer = CCharVectorFunctions<TChar>::ToChar(&obj.front());
    value.insert(value.end(), buffer, buffer + obj.size());
}

template<typename Char>
void CCharVectorTypeInfo<Char>::SetValueOctetString(TObjectPtr objectPtr,
                                                    const vector<char>& value) const
{
    TObjectType& obj = CCharVectorFunctions<TChar>::Get(objectPtr);
    obj.clear();
    const TChar* buffer = CCharVectorFunctions<TChar>::ToTChar(&value.front());
    obj.insert(obj.end(), buffer, buffer + value.size());
}

TTypeInfo CStdTypeInfo< vector<char> >::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CCharVectorTypeInfo<char>;
    return typeInfo;
}

TTypeInfo CStdTypeInfo< vector<signed char> >::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CCharVectorTypeInfo<signed char>;
    return typeInfo;
}

TTypeInfo CStdTypeInfo< vector<unsigned char> >::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CCharVectorTypeInfo<unsigned char>;
    return typeInfo;
}

END_NCBI_SCOPE
