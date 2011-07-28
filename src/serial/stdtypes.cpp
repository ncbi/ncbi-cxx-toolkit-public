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
*   CTypeInfo classes for primitive leaf types.
*/

#include <ncbi_pch.hpp>
#include <serial/serialdef.hpp>
#include <serial/impl/stdtypesimpl.hpp>
#include <serial/impl/typeinfoimpl.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>

#include <limits.h>
#if HAVE_WINDOWS_H
// In MSVC limits.h doesn't define FLT_MIN & FLT_MAX
# include <float.h>
#endif
#include <math.h>

BEGIN_NCBI_SCOPE


class CPrimitiveTypeFunctionsBase
{
public:
    static bool IsNegative(Int4 value)
        {
            return value < 0;
        }
    static bool IsNegative(Uint4 /*value*/)
        {
            return false;
        }
#if SIZEOF_LONG == 4
    // add variants with long to avoid ambiguity
    static bool IsNegative(long value)
        {
            return value < 0;
        }
    static bool IsNegative(unsigned long /*value*/)
        {
            return false;
        }
#endif
    static bool IsNegative(Int8 value)
        {
            return value < 0;
        }
    static bool IsNegative(Uint8 /*value*/)
        {
            return false;
        }
};

template<typename T>
class CPrimitiveTypeFunctions : public CPrimitiveTypeFunctionsBase
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

    static TObjectPtr Create(TTypeInfo /*typeInfo*/,
                             CObjectMemoryPool* /*memoryPool*/)
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

    static bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2,
                       ESerialRecursionMode)
        {
            return Get(obj1) == Get(obj2);
        }
    static void Assign(TObjectPtr dst, TConstObjectPtr src,
                       ESerialRecursionMode)
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

#define EPSILON_(n) 1e-##n
#define EPSILON(n) EPSILON_(n)

#ifndef DBL_EPSILON
#  define DBL_EPSILON EPSILON(DBL_DIG)
#endif

EMPTY_TEMPLATE
bool CPrimitiveTypeFunctions<double>::Equals(TConstObjectPtr obj1,
                                             TConstObjectPtr obj2,
                                             ESerialRecursionMode)
{
    const double& x = Get(obj1);
    const double& y = Get(obj2);
    return (x == y  ||  fabs(x - y) < fabs(x + y) * DBL_EPSILON);
}

#ifndef FLT_EPSILON
#  define FLT_EPSILON EPSILON(FLT_DIG)
#endif

EMPTY_TEMPLATE
bool CPrimitiveTypeFunctions<float>::Equals(TConstObjectPtr obj1,
                                            TConstObjectPtr obj2,
                                            ESerialRecursionMode)
{
    const float& x = Get(obj1);
    const float& y = Get(obj2);
    return (x == y  ||  fabs(x - y) < fabs(x + y) * FLT_EPSILON);
}

#if SIZEOF_LONG_DOUBLE != 0

EMPTY_TEMPLATE
bool CPrimitiveTypeFunctions<long double>::Equals(TConstObjectPtr obj1,
                                                  TConstObjectPtr obj2,
                                                  ESerialRecursionMode)
{
    const long double& x = Get(obj1);
    const long double& y = Get(obj2);
    // We use DBL_EPSILON because I/O is double-based anyway.
    return (x == y  ||  fabs(x - y) < fabs(x + y) * DBL_EPSILON);
}
#endif

void CVoidTypeFunctions::ThrowException(const char* operation,
                                        TTypeInfo objectType)
{
    string message("cannot ");
    message += operation;
    message += " object of type: ";
    message += objectType->GetName();
    NCBI_THROW(CSerialException,eIllegalCall, message);
}

bool CVoidTypeFunctions::IsDefault(TConstObjectPtr )
{
    return true;
}

bool CVoidTypeFunctions::Equals(TConstObjectPtr , TConstObjectPtr,
                                ESerialRecursionMode )
{
    ThrowIllegalCall();
    return false;
}

void CVoidTypeFunctions::SetDefault(TObjectPtr )
{
}

void CVoidTypeFunctions::Assign(TObjectPtr , TConstObjectPtr, ESerialRecursionMode )
{
    ThrowIllegalCall();
}

void CVoidTypeFunctions::Read(CObjectIStream& in, TTypeInfo ,
                              TObjectPtr )
{
    in.ThrowError(in.fIllegalCall,
                  "CVoidTypeFunctions::Read cannot read");
}

void CVoidTypeFunctions::Write(CObjectOStream& out, TTypeInfo ,
                               TConstObjectPtr )
{
    out.ThrowError(out.fIllegalCall,
                   "CVoidTypeFunctions::Write cannot write");
}

void CVoidTypeFunctions::Copy(CObjectStreamCopier& copier, TTypeInfo )
{
    copier.ThrowError(CObjectIStream::fIllegalCall,
                      "CVoidTypeFunctions::Copy cannot copy");
}

void CVoidTypeFunctions::Skip(CObjectIStream& in, TTypeInfo )
{
    in.ThrowError(in.fIllegalCall,
                  "CVoidTypeFunctions::Skip cannot skip");
}

TObjectPtr CVoidTypeFunctions::Create(TTypeInfo objectType,
                                      CObjectMemoryPool* /*memoryPool*/)
{
    ThrowException("CVoidTypeFunctions::Create cannot create", objectType);
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

bool CPrimitiveTypeInfo::Equals(TConstObjectPtr obj1, TConstObjectPtr obj2,
                                ESerialRecursionMode how) const
{
    return m_Equals(obj1, obj2, how);
}

void CPrimitiveTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src,
                                ESerialRecursionMode how) const
{
    m_Assign(dst, src, how);
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

Int4 CPrimitiveTypeInfo::GetValueInt4(TConstObjectPtr /*objectPtr*/) const
{
    ThrowIncompatibleValue();
    return 0;
}

void CPrimitiveTypeInfo::SetValueInt4(TObjectPtr /*objectPtr*/,
                                      Int4 /*value*/) const
{
    ThrowIncompatibleValue();
}

Uint4 CPrimitiveTypeInfo::GetValueUint4(TConstObjectPtr /*objectPtr*/) const
{
    ThrowIncompatibleValue();
    return 0;
}

void CPrimitiveTypeInfo::SetValueUint4(TObjectPtr /*objectPtr*/,
                                       Uint4 /*value*/) const
{
    ThrowIncompatibleValue();
}

Int8 CPrimitiveTypeInfo::GetValueInt8(TConstObjectPtr /*objectPtr*/) const
{
    ThrowIncompatibleValue();
    return 0;
}

void CPrimitiveTypeInfo::SetValueInt8(TObjectPtr /*objectPtr*/,
                                      Int8 /*value*/) const
{
    ThrowIncompatibleValue();
}

Uint8 CPrimitiveTypeInfo::GetValueUint8(TConstObjectPtr /*objectPtr*/) const
{
    ThrowIncompatibleValue();
    return 0;
}

void CPrimitiveTypeInfo::SetValueUint8(TObjectPtr /*objectPtr*/,
                                       Uint8 /*value*/) const
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

#if SIZEOF_LONG_DOUBLE != 0
long double CPrimitiveTypeInfo::GetValueLDouble(TConstObjectPtr objectPtr) const
{
    return GetValueDouble(objectPtr);
}

void CPrimitiveTypeInfo::SetValueLDouble(TObjectPtr objectPtr,
                                         long double value) const
{
    SetValueDouble(objectPtr, value);
}
#endif

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
                                             const vector<char>& /*value*/)
    const
{
    ThrowIncompatibleValue();
}

void CPrimitiveTypeInfo::GetValueBitString(TConstObjectPtr /*objectPtr*/,
                                           CBitString& /*value*/) const
{
    ThrowIncompatibleValue();
}
void CPrimitiveTypeInfo::SetValueBitString(TObjectPtr /*objectPtr*/,
                                           const CBitString& /*value*/) const
{
    ThrowIncompatibleValue();
}

void CPrimitiveTypeInfo::GetValueAnyContent(TConstObjectPtr /*objectPtr*/,
                                            CAnyContentObject& /*value*/) const
{
    ThrowIncompatibleValue();
}

void CPrimitiveTypeInfo::SetValueAnyContent(TObjectPtr /*objectPtr*/,
                                            const CAnyContentObject& /*value*/)
    const
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
    static TTypeInfo info = CreateTypeInfo();
    return info;
}

CTypeInfo* CStdTypeInfo<bool>::CreateTypeInfo(void)
{
    return new CPrimitiveTypeInfoBool();
}

class CNullBoolFunctions : public CPrimitiveTypeFunctions<bool>
{
public:
    static TObjectPtr Create(TTypeInfo /* type */,
                             CObjectMemoryPool* /*memoryPool*/)
        {
            NCBI_THROW(CSerialException,eIllegalCall,
                       "Cannot create NULL object");
            return 0;
        }
    static bool IsDefault(TConstObjectPtr)
        {
            return false;
        }
    static void SetDefault(TObjectPtr)
        {
        }

    static bool Equals(TConstObjectPtr, TConstObjectPtr,
                       ESerialRecursionMode)
        {
            return true;
        }
    static void Assign(TObjectPtr, TConstObjectPtr,
                       ESerialRecursionMode)
        {
        }

    static void Read(CObjectIStream& in, TTypeInfo ,
                     TObjectPtr /* object */)
        {
            in.ReadNull();
        }
    static void Write(CObjectOStream& out, TTypeInfo ,
                      TConstObjectPtr /* object */)
        {
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
    static TTypeInfo info = CreateTypeInfoNullBool();
    return info;
}

CTypeInfo* CStdTypeInfo<bool>::CreateTypeInfoNullBool(void)
{
    CNullTypeInfo* info = new CNullTypeInfo();
    typedef CNullBoolFunctions TFunctions;
    info->SetMemFunctions(&TFunctions::Create, &TFunctions::IsDefault,
                          &TFunctions::SetDefault,&TFunctions::Equals,
                          &TFunctions::Assign);
    info->SetIOFunctions(&TFunctions::Read, &TFunctions::Write,
                         &TFunctions::Copy, &TFunctions::Skip);
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
    static TTypeInfo info = CreateTypeInfo();
    return info;
}

CTypeInfo* CStdTypeInfo<char>::CreateTypeInfo(void)
{
    return new CPrimitiveTypeInfoChar();
}

template<typename T>
class CPrimitiveTypeInfoIntFunctions : public CPrimitiveTypeFunctions<T>
{
    typedef CPrimitiveTypeFunctions<T> CParent;
public:
    typedef T TObjectType;
    
    static CPrimitiveTypeInfoInt* CreateTypeInfo(void)
        {
            CPrimitiveTypeInfoInt* info =
                new CPrimitiveTypeInfoInt(sizeof(TObjectType), IsSigned());

            info->SetMemFunctions(&CParent::Create,
                                  &IsDefault, &SetDefault, &Equals, &Assign);

            info->SetIOFunctions(&CParent::Read, &CParent::Write,
                                 &CParent::Copy, &CParent::Skip);

            SetInt4Functions(info);
            SetInt8Functions(info);
            return info;
        }

    static void SetInt4Functions(CPrimitiveTypeInfoInt* info)
        {
            info->SetInt4Functions(&GetValueInt4, &SetValueInt4,
                                  &GetValueUint4, &SetValueUint4);
        }

    static void SetInt8Functions(CPrimitiveTypeInfoInt* info)
        {
            info->SetInt8Functions(&GetValueInt8, &SetValueInt8,
                                  &GetValueUint8, &SetValueUint8);
        }

    static bool IsDefault(TConstObjectPtr objectPtr)
        {
            return CParent::Get(objectPtr) == 0;
        }
    static void SetDefault(TObjectPtr objectPtr)
        {
            CParent::Get(objectPtr) = 0;
        }
    static bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2,
                       ESerialRecursionMode)
        {
            return CParent::Get(obj1) == CParent::Get(obj2);
        }
    static void Assign(TObjectPtr dst, TConstObjectPtr src,
                       ESerialRecursionMode)
        {
            CParent::Get(dst) = CParent::Get(src);
        }

    static bool IsSigned(void)
        {
            return TObjectType(-1) < 0;
        }
    static bool IsUnsigned(void)
        {
            return TObjectType(-1) > 0;
        }

    static Int4 GetValueInt4(TConstObjectPtr objectPtr)
        {
            TObjectType value = CParent::Get(objectPtr);
            Int4 result = Int4(value);
            if ( IsUnsigned() ) {
                // unsigned -> signed
                if ( sizeof(value) == sizeof(result) ) {
                    // same size - check for sign change only
                    if ( CParent::IsNegative(result) )
                        ThrowIntegerOverflow();
                }
            }
            if ( sizeof(value) > sizeof(result) ) {
                if ( value != TObjectType(result) )
                    ThrowIntegerOverflow();
            }
            return result;
        }
    static Uint4 GetValueUint4(TConstObjectPtr objectPtr)
        {
            TObjectType value = CParent::Get(objectPtr);
            Uint4 result = Uint4(value);
            if ( IsSigned() ) {
                // signed -> unsigned
                // check for negative value
                if ( IsNegative(value) )
                    ThrowIntegerOverflow();
            }
            if ( sizeof(value) > sizeof(result) ) {
                if ( value != TObjectType(result) )
                    ThrowIntegerOverflow();
            }
            return result;
        }
    static void SetValueInt4(TObjectPtr objectPtr, Int4 value)
        {
            TObjectType result = TObjectType(value);
            if ( IsUnsigned() ) {
                // signed -> unsigned
                // check for negative value
                if ( CParent::IsNegative(value) )
                    ThrowIntegerOverflow();
            }
            if ( sizeof(value) > sizeof(result) ) {
                if ( value != Int4(result) )
                    ThrowIntegerOverflow();
            }
            CParent::Get(objectPtr) = result;
        }
    static void SetValueUint4(TObjectPtr objectPtr, Uint4 value)
        {
            TObjectType result = TObjectType(value);
            if ( IsSigned() ) {
                // unsigned -> signed
                if ( sizeof(value) == sizeof(result) ) {
                    // same size - check for sign change only
                    if ( IsNegative(result) )
                        ThrowIntegerOverflow();
                }
            }
            if ( sizeof(value) > sizeof(result) ) {
                if ( value != Uint4(result) )
                    ThrowIntegerOverflow();
            }
            CParent::Get(objectPtr) = result;
        }
    static Int8 GetValueInt8(TConstObjectPtr objectPtr)
        {
            TObjectType value = CParent::Get(objectPtr);
            Int8 result = Int8(value);
            if ( IsUnsigned() ) {
                // unsigned -> signed
                if ( sizeof(value) == sizeof(result) ) {
                    // same size - check for sign change only
                    if ( CParent::IsNegative(result) )
                        ThrowIntegerOverflow();
                }
            }
            if ( sizeof(value) > sizeof(result) ) {
                if ( value != TObjectType(result) )
                    ThrowIntegerOverflow();
            }
            return result;
        }
    static Uint8 GetValueUint8(TConstObjectPtr objectPtr)
        {
            TObjectType value = CParent::Get(objectPtr);
            Uint8 result = Uint8(value);
            if ( IsSigned() ) {
                // signed -> unsigned
                // check for negative value
                if ( IsNegative(value) )
                    ThrowIntegerOverflow();
            }
            if ( sizeof(value) > sizeof(result) ) {
                if ( value != TObjectType(result) )
                    ThrowIntegerOverflow();
            }
            return result;
        }
    static void SetValueInt8(TObjectPtr objectPtr, Int8 value)
        {
            TObjectType result = TObjectType(value);
            if ( IsUnsigned() ) {
                // signed -> unsigned
                // check for negative value
                if ( CParent::IsNegative(value) )
                    ThrowIntegerOverflow();
            }
            if ( sizeof(value) > sizeof(result) ) {
                if ( value != Int8(result) )
                    ThrowIntegerOverflow();
            }
            CParent::Get(objectPtr) = result;
        }
    static void SetValueUint8(TObjectPtr objectPtr, Uint8 value)
        {
            TObjectType result = TObjectType(value);
            if ( IsSigned() ) {
                // unsigned -> signed
                if ( sizeof(value) == sizeof(result) ) {
                    // same size - check for sign change only
                    if ( IsNegative(result) )
                        ThrowIntegerOverflow();
                }
            }
            if ( sizeof(value) > sizeof(result) ) {
                if ( value != Uint8(result) )
                    ThrowIntegerOverflow();
            }
            CParent::Get(objectPtr) = result;
        }
};

CPrimitiveTypeInfoInt::CPrimitiveTypeInfoInt(size_t size, bool isSigned)
    : CParent(size, ePrimitiveValueInteger, isSigned)
{
}

void CPrimitiveTypeInfoInt::SetInt4Functions(TGetInt4Function getInt4,
                                             TSetInt4Function setInt4,
                                             TGetUint4Function getUint4,
                                             TSetUint4Function setUint4)
{
    m_GetInt4 = getInt4;
    m_SetInt4 = setInt4;
    m_GetUint4 = getUint4;
    m_SetUint4 = setUint4;
}

void CPrimitiveTypeInfoInt::SetInt8Functions(TGetInt8Function getInt8,
                                             TSetInt8Function setInt8,
                                             TGetUint8Function getUint8,
                                             TSetUint8Function setUint8)
{
    m_GetInt8 = getInt8;
    m_SetInt8 = setInt8;
    m_GetUint8 = getUint8;
    m_SetUint8 = setUint8;
}

Int4 CPrimitiveTypeInfoInt::GetValueInt4(TConstObjectPtr objectPtr) const
{
    return m_GetInt4(objectPtr);
}

Uint4 CPrimitiveTypeInfoInt::GetValueUint4(TConstObjectPtr objectPtr) const
{
    return m_GetUint4(objectPtr);
}

void CPrimitiveTypeInfoInt::SetValueInt4(TObjectPtr objectPtr,
                                         Int4 value) const
{
    m_SetInt4(objectPtr, value);
}

void CPrimitiveTypeInfoInt::SetValueUint4(TObjectPtr objectPtr,
                                          Uint4 value) const
{
    m_SetUint4(objectPtr, value);
}

Int8 CPrimitiveTypeInfoInt::GetValueInt8(TConstObjectPtr objectPtr) const
{
    return m_GetInt8(objectPtr);
}

Uint8 CPrimitiveTypeInfoInt::GetValueUint8(TConstObjectPtr objectPtr) const
{
    return m_GetUint8(objectPtr);
}

void CPrimitiveTypeInfoInt::SetValueInt8(TObjectPtr objectPtr,
                                         Int8 value) const
{
    m_SetInt8(objectPtr, value);
}

void CPrimitiveTypeInfoInt::SetValueUint8(TObjectPtr objectPtr,
                                          Uint8 value) const
{
    m_SetUint8(objectPtr, value);
}

#define DECLARE_STD_INT_TYPE(Type) \
TTypeInfo CStdTypeInfo<Type>::GetTypeInfo(void) \
{ \
    static TTypeInfo info = CreateTypeInfo(); \
    return info; \
} \
CTypeInfo* CStdTypeInfo<Type>::CreateTypeInfo(void) \
{ \
    return CPrimitiveTypeInfoIntFunctions<Type>::CreateTypeInfo(); \
}

DECLARE_STD_INT_TYPE(signed char)
DECLARE_STD_INT_TYPE(unsigned char)
DECLARE_STD_INT_TYPE(short)
DECLARE_STD_INT_TYPE(unsigned short)
DECLARE_STD_INT_TYPE(int)
DECLARE_STD_INT_TYPE(unsigned)
#if SIZEOF_LONG == 4
DECLARE_STD_INT_TYPE(long)
DECLARE_STD_INT_TYPE(unsigned long)
#endif
DECLARE_STD_INT_TYPE(Int8)
DECLARE_STD_INT_TYPE(Uint8)

const CPrimitiveTypeInfo* CPrimitiveTypeInfo::GetIntegerTypeInfo(size_t size,
                                                                 bool sign)
{
    TTypeInfo info;
    if ( size == sizeof(int) ) {
        if ( sign )
            info = CStdTypeInfo<int>::GetTypeInfo();
        else
            info = CStdTypeInfo<unsigned>::GetTypeInfo();
    }
    else if ( size == sizeof(short) ) {
        if ( sign )
            info = CStdTypeInfo<short>::GetTypeInfo();
        else
            info = CStdTypeInfo<unsigned short>::GetTypeInfo();
    }
    else if ( size == sizeof(signed char) ) {
        if ( sign )
            info = CStdTypeInfo<signed char>::GetTypeInfo();
        else
            info = CStdTypeInfo<unsigned char>::GetTypeInfo();
    }
    else if ( size == sizeof(Int8) ) {
        if ( sign )
            info = CStdTypeInfo<Int8>::GetTypeInfo();
        else
            info = CStdTypeInfo<Uint8>::GetTypeInfo();
    }
    else {
        string message("Illegal enum size: ");
        message += NStr::SizetToString(size);
        NCBI_THROW(CSerialException,eInvalidData, message);
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

void CPrimitiveTypeInfoDouble::SetValueDouble(TObjectPtr objectPtr,
                                              double value) const
{
    CPrimitiveTypeFunctions<TObjectType>::Get(objectPtr) = value;
}

TTypeInfo CStdTypeInfo<double>::GetTypeInfo(void)
{
    static TTypeInfo info = CreateTypeInfo();
    return info;
}

CTypeInfo* CStdTypeInfo<double>::CreateTypeInfo(void)
{
    return new CPrimitiveTypeInfoDouble();
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

void CPrimitiveTypeInfoFloat::SetValueDouble(TObjectPtr objectPtr,
                                             double value) const
{
#if defined(FLT_MIN) && defined(FLT_MAX)
    if ( value < FLT_MIN || value > FLT_MAX )
        ThrowIncompatibleValue();
#endif
    CPrimitiveTypeFunctions<TObjectType>::Get(objectPtr) = TObjectType(value);
}

TTypeInfo CStdTypeInfo<float>::GetTypeInfo(void)
{
    static TTypeInfo info = CreateTypeInfo();
    return info;
}

CTypeInfo* CStdTypeInfo<float>::CreateTypeInfo(void)
{
    return new CPrimitiveTypeInfoFloat();
}

#if SIZEOF_LONG_DOUBLE != 0
CPrimitiveTypeInfoLongDouble::CPrimitiveTypeInfoLongDouble(void)
    : CParent(sizeof(TObjectType), ePrimitiveValueReal, true)
{
    CPrimitiveTypeFunctions<TObjectType>::SetMemFunctions(this);
    CPrimitiveTypeFunctions<TObjectType>::SetIOFunctions(this);
}

double CPrimitiveTypeInfoLongDouble::GetValueDouble(TConstObjectPtr objectPtr) const
{
    return CPrimitiveTypeFunctions<TObjectType>::Get(objectPtr);
}

void CPrimitiveTypeInfoLongDouble::SetValueDouble(TObjectPtr objectPtr,
                                               double value) const
{
    CPrimitiveTypeFunctions<TObjectType>::Get(objectPtr) = TObjectType(value);
}

long double CPrimitiveTypeInfoLongDouble::GetValueLDouble(TConstObjectPtr objectPtr) const
{
    return CPrimitiveTypeFunctions<TObjectType>::Get(objectPtr);
}

void CPrimitiveTypeInfoLongDouble::SetValueLDouble(TObjectPtr objectPtr,
                                                   long double value) const
{
    CPrimitiveTypeFunctions<TObjectType>::Get(objectPtr) = TObjectType(value);
}

TTypeInfo CStdTypeInfo<long double>::GetTypeInfo(void)
{
    static TTypeInfo info = CreateTypeInfo();
    return info;
}

CTypeInfo* CStdTypeInfo<long double>::CreateTypeInfo(void)
{
    return new CPrimitiveTypeInfoLongDouble();
}
#endif

template<typename T>
class CStringFunctions : public CPrimitiveTypeFunctions<T>
{
    typedef CPrimitiveTypeFunctions<T> CParent;
public:
    static TObjectPtr Create(TTypeInfo /*typeInfo*/,
                             CObjectMemoryPool* /*memoryPool*/)
        {
            return new T();
        }
    static bool IsDefault(TConstObjectPtr objectPtr)
        {
            return CParent::Get(objectPtr).empty();
        }
    static void SetDefault(TObjectPtr objectPtr)
        {
            CParent::Get(objectPtr).erase();
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

CPrimitiveTypeInfoString::CPrimitiveTypeInfoString(EType type)
    : CParent(sizeof(string), ePrimitiveValueString), m_Type(type)
{
    if (type == eStringTypeUTF8) {
        SetMemFunctions(&CStringFunctions<CStringUTF8>::Create,
                        &CStringFunctions<CStringUTF8>::IsDefault,
                        &CStringFunctions<CStringUTF8>::SetDefault,
                        &CStringFunctions<CStringUTF8>::Equals,
                        &CStringFunctions<CStringUTF8>::Assign);
        SetIOFunctions(&CStringFunctions<CStringUTF8>::Read,
                       &CStringFunctions<CStringUTF8>::Write,
                       &CStringFunctions<CStringUTF8>::Copy,
                       &CStringFunctions<CStringUTF8>::Skip);
    } else {
        SetMemFunctions(&CStringFunctions<string>::Create,
                        &CStringFunctions<string>::IsDefault,
                        &CStringFunctions<string>::SetDefault,
                        &CStringFunctions<string>::Equals,
                        &CStringFunctions<string>::Assign);
        SetIOFunctions(&CStringFunctions<string>::Read,
                       &CStringFunctions<string>::Write,
                       &CStringFunctions<string>::Copy,
                       &CStringFunctions<string>::Skip);
    }
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
    static TTypeInfo info = CreateTypeInfo();
    return info;
}

CTypeInfo* CStdTypeInfo<string>::CreateTypeInfo(void)
{
    return new CPrimitiveTypeInfoString();
}

TTypeInfo CStdTypeInfo<ncbi::CStringUTF8>::GetTypeInfo(void)
{
    static TTypeInfo info = CreateTypeInfo();
    return info;
}

CTypeInfo* CStdTypeInfo<ncbi::CStringUTF8>::CreateTypeInfo(void)
{
    return new CPrimitiveTypeInfoString(CPrimitiveTypeInfoString::eStringTypeUTF8);
}

class CStringStoreFunctions : public CStringFunctions<string>
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
    static TTypeInfo info = CreateTypeInfoStringStore();
    return info;
}

CTypeInfo* CStdTypeInfo<string>::CreateTypeInfoStringStore(void)
{
    CPrimitiveTypeInfo* info = new CPrimitiveTypeInfoString;
    typedef CStringStoreFunctions TFunctions;
    info->SetIOFunctions(&TFunctions::Read, &TFunctions::Write,
                         &TFunctions::Copy, &TFunctions::Skip);
    return info;
}

template<typename T>
class CCharPtrFunctions : public CPrimitiveTypeFunctions<T>
{
    typedef CPrimitiveTypeFunctions<T> CParent;
public:
    static bool IsDefault(TConstObjectPtr objectPtr)
        {
            return CParent::Get(objectPtr) == 0;
        }
    static void SetDefault(TObjectPtr dst)
        {
            free(const_cast<char*>(CParent::Get(dst)));
            CParent::Get(dst) = 0;
        }
    static bool Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                       ESerialRecursionMode)
        {
            return strcmp(CParent::Get(object1), CParent::Get(object2)) == 0;
        }
    static void Assign(TObjectPtr dst, TConstObjectPtr src,
                       ESerialRecursionMode)
        {
            typename CPrimitiveTypeFunctions<T>::TObjectType value
                = CParent::Get(src);
            _ASSERT(CParent::Get(dst) != value);
            free(const_cast<char*>(CParent::Get(dst)));
            if ( value )
                CParent::Get(dst) = NotNull(strdup(value));
            else
                CParent::Get(dst) = 0;
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
    static TTypeInfo info = CreateTypeInfo();
    return info;
}

CTypeInfo* CStdTypeInfo<char*>::CreateTypeInfo(void)
{
    return new CPrimitiveTypeInfoCharPtr<char*>();
}

TTypeInfo CStdTypeInfo<const char*>::GetTypeInfo(void)
{
    static TTypeInfo info = CreateTypeInfo();
    return info;
}

CTypeInfo* CStdTypeInfo<const char*>::CreateTypeInfo(void)
{
    return new CPrimitiveTypeInfoCharPtr<const char*>();
}

void ThrowIncompatibleValue(void)
{
    NCBI_THROW(CSerialException,eInvalidData, "incompatible value");
}

void ThrowIllegalCall(void)
{
    NCBI_THROW(CSerialException,eIllegalCall, "illegal call");
}

void ThrowIntegerOverflow(void)
{
    NCBI_THROW(CSerialException,eOverflow, "integer overflow");
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

    static TObjectPtr Create(TTypeInfo /*typeInfo*/,
                             CObjectMemoryPool* /*memoryPool*/)
        {
            return new TObjectType();
        }
    static bool IsDefault(TConstObjectPtr object)
        {
            return Get(object).empty();
        }
    static bool Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                       ESerialRecursionMode)
        {
            return Get(object1) == Get(object2);
        }
    static void SetDefault(TObjectPtr dst)
        {
            Get(dst).clear();
        }
    static void Assign(TObjectPtr dst, TConstObjectPtr src,
                       ESerialRecursionMode)
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
#if 1
                o.clear();
                o.reserve(length);
                Char buf[2048];
                size_t count;
                while ( (count = block.Read(ToChar(buf), sizeof(buf))) != 0 ) {
                    o.insert(o.end(), buf, buf + count);
                }
#else
                o.resize(length);
                block.Read(ToChar(&o.front()), length, true);
#endif
            }
            else {
                // length is unknown -> copy via buffer
                o.clear();
                Char buf[4096];
                size_t count;
                while ( (count = block.Read(ToChar(buf), sizeof(buf))) != 0 ) {
#ifdef RESERVE_VECTOR_SIZE
                    size_t new_size = o.size() + count;
                    if ( new_size > o.capacity() ) {
                        o.reserve(RESERVE_VECTOR_SIZE(new_size));
                    }
#endif
                    o.insert(o.end(), buf, buf + count);
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
    if (!obj.empty()) {
        const char* buffer = CCharVectorFunctions<TChar>::ToChar(&obj.front());
        value.assign(buffer, buffer + obj.size());
    }
}

template<typename Char>
void CCharVectorTypeInfo<Char>::SetValueString(TObjectPtr objectPtr,
                                               const string& value) const
{
    TObjectType& obj = CCharVectorFunctions<TChar>::Get(objectPtr);
    obj.clear();
    if (!value.empty()) {
        const TChar* buffer = CCharVectorFunctions<TChar>::ToTChar(value.data());
        obj.insert(obj.end(), buffer, buffer + value.size());
    }
}

template<typename Char>
void CCharVectorTypeInfo<Char>::GetValueOctetString(TConstObjectPtr objectPtr,
                                                    vector<char>& value) const
{
    const TObjectType& obj = CCharVectorFunctions<TChar>::Get(objectPtr);
    value.clear();
    if (!obj.empty()) {
        const char* buffer = CCharVectorFunctions<TChar>::ToChar(&obj.front());
        value.insert(value.end(), buffer, buffer + obj.size());
    }
}

template<typename Char>
void CCharVectorTypeInfo<Char>::SetValueOctetString(TObjectPtr objectPtr,
                                                    const vector<char>& value) const
{
    TObjectType& obj = CCharVectorFunctions<TChar>::Get(objectPtr);
    obj.clear();
    if (!value.empty()) {
        const TChar* buffer = CCharVectorFunctions<TChar>::ToTChar(&value.front());
        obj.insert(obj.end(), buffer, buffer + value.size());
    }
}

TTypeInfo CStdTypeInfo< vector<char> >::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = CreateTypeInfo();
    return typeInfo;
}

TTypeInfo CStdTypeInfo< vector<signed char> >::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = CreateTypeInfo();
    return typeInfo;
}

TTypeInfo CStdTypeInfo< vector<unsigned char> >::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = CreateTypeInfo();
    return typeInfo;
}

CTypeInfo* CStdTypeInfo< vector<char> >::CreateTypeInfo(void)
{
    return new CCharVectorTypeInfo<char>;
}

CTypeInfo* CStdTypeInfo< vector<signed char> >::CreateTypeInfo(void)
{
    return new CCharVectorTypeInfo<signed char>;
}

CTypeInfo* CStdTypeInfo< vector<unsigned char> >::CreateTypeInfo(void)
{
    return new CCharVectorTypeInfo<unsigned char>;
}


class CAnyContentFunctions : public CPrimitiveTypeFunctions<ncbi::CAnyContentObject>
{
public:
    static TObjectPtr Create(TTypeInfo /*typeInfo*/,
                             CObjectMemoryPool* /*memoryPool*/)
        {
            return new TObjectType();
        }
    static bool IsDefault(TConstObjectPtr objectPtr)
        {
            return Get(objectPtr) == TObjectType();
        }
    static void SetDefault(TObjectPtr objectPtr)
        {
            Get(objectPtr) = TObjectType();
        }
    static void Read(CObjectIStream& in, TTypeInfo , TObjectPtr objectPtr)
        {
            in.ReadAnyContentObject(Get(objectPtr));
        }
    static void Write(CObjectOStream& out, TTypeInfo ,
                      TConstObjectPtr objectPtr)
        {
            out.WriteAnyContentObject(Get(objectPtr));
        }
    static void Copy(CObjectStreamCopier& copier, TTypeInfo )
        {
            copier.CopyAnyContentObject();
        }
    static void Skip(CObjectIStream& in, TTypeInfo )
        {
            in.SkipAnyContentObject();
        }
};

CPrimitiveTypeInfoAnyContent::CPrimitiveTypeInfoAnyContent(void)
    : CParent(sizeof(CAnyContentObject), ePrimitiveValueAny)
{
    m_IsCObject = true;
    typedef CPrimitiveTypeFunctions<ncbi::CAnyContentObject> TFunctions;
    SetMemFunctions(&CAnyContentFunctions::Create,
                    &CAnyContentFunctions::IsDefault,
                    &CAnyContentFunctions::SetDefault,
                    &TFunctions::Equals, &TFunctions::Assign);
    SetIOFunctions(&CAnyContentFunctions::Read,
                   &CAnyContentFunctions::Write,
                   &CAnyContentFunctions::Copy,
                   &CAnyContentFunctions::Skip);
}

TTypeInfo CStdTypeInfo<CAnyContentObject>::GetTypeInfo(void)
{
    static TTypeInfo info = CreateTypeInfo();
    return info;
}

CTypeInfo* CStdTypeInfo<ncbi::CAnyContentObject>::CreateTypeInfo(void)
{
    return new CPrimitiveTypeInfoAnyContent();
}

void
CPrimitiveTypeInfoAnyContent::GetValueAnyContent(TConstObjectPtr objectPtr,
                                                 CAnyContentObject& value)
    const
{
    typedef CPrimitiveTypeFunctions<ncbi::CAnyContentObject> TFunctions;
    value = TFunctions::Get(objectPtr);
}

void
CPrimitiveTypeInfoAnyContent::SetValueAnyContent(TObjectPtr objectPtr,
                                                 const CAnyContentObject& value)
    const
{
    typedef CPrimitiveTypeFunctions<ncbi::CAnyContentObject> TFunctions;
    TFunctions::Get(objectPtr) = value;
}


class CBitStringFunctions : public CPrimitiveTypeFunctions<CBitString>
{
public:
    static TObjectPtr Create(TTypeInfo /*typeInfo*/,
                             CObjectMemoryPool* /*memoryPool*/)
        {
            return new TObjectType();
        }
    static bool IsDefault(TConstObjectPtr objectPtr)
        {
            return Get(objectPtr) == TObjectType();
        }
    static void SetDefault(TObjectPtr objectPtr)
        {
            Get(objectPtr) = TObjectType();
        }
};

CPrimitiveTypeInfoBitString::CPrimitiveTypeInfoBitString(void)
    : CParent(sizeof(CBitString), ePrimitiveValueBitString)
{
    typedef CPrimitiveTypeFunctions<ncbi::CBitString> TFunctions;
    SetMemFunctions(&CBitStringFunctions::Create,
                    &CBitStringFunctions::IsDefault,
                    &CBitStringFunctions::SetDefault,
                    &TFunctions::Equals,
                    &TFunctions::Assign);
    SetIOFunctions(&TFunctions::Read,
                   &TFunctions::Write,
                   &TFunctions::Copy,
                   &TFunctions::Skip);
//    CPrimitiveTypeFunctions<CBitString>::SetMemFunctions(this);
//    CPrimitiveTypeFunctions<CBitString>::SetIOFunctions(this);
}

TTypeInfo CStdTypeInfo<CBitString>::GetTypeInfo(void)
{
    static TTypeInfo info = CreateTypeInfo();
    return info;
}

CTypeInfo* CStdTypeInfo<CBitString>::CreateTypeInfo(void)
{
    return new CPrimitiveTypeInfoBitString();
}

void CPrimitiveTypeInfoBitString::GetValueBitString(TConstObjectPtr objectPtr,
                                                    CBitString& value) const
{
    typedef CPrimitiveTypeFunctions<ncbi::CBitString> TFunctions;
    value = TFunctions::Get(objectPtr);
}

void CPrimitiveTypeInfoBitString::SetValueBitString(TObjectPtr objectPtr,
                                                    const CBitString& value)
    const
{
    typedef CPrimitiveTypeFunctions<ncbi::CBitString> TFunctions;
    TFunctions::Get(objectPtr) = value;
}

END_NCBI_SCOPE
