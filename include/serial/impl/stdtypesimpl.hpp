#ifndef STDTYPESIMPL__HPP
#define STDTYPESIMPL__HPP

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

#include <corelib/ncbistd.hpp>
#include <serial/impl/stdtypes.hpp>
#include <serial/objcopy.hpp>
#include <serial/serialutil.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

// throw various exceptions
NCBI_XSERIAL_EXPORT NCBI_NORETURN
void ThrowIntegerOverflow(void);

NCBI_XSERIAL_EXPORT NCBI_NORETURN
void ThrowIncompatibleValue(void);

NCBI_XSERIAL_EXPORT NCBI_NORETURN
void ThrowIllegalCall(void);

#define SERIAL_ENUMERATE_STD_TYPE1(Type) SERIAL_ENUMERATE_STD_TYPE(Type, Type)

#define SERIAL_ENUMERATE_ALL_CHAR_TYPES \
    SERIAL_ENUMERATE_STD_TYPE1(char) \
    SERIAL_ENUMERATE_STD_TYPE(signed char, schar) \
    SERIAL_ENUMERATE_STD_TYPE(unsigned char, uchar)

#if SIZEOF_LONG == 4
#define SERIAL_ENUMERATE_ALL_INTEGRAL_TYPES \
    SERIAL_ENUMERATE_STD_TYPE1(short) \
    SERIAL_ENUMERATE_STD_TYPE(unsigned short, ushort) \
    SERIAL_ENUMERATE_STD_TYPE1(int) \
    SERIAL_ENUMERATE_STD_TYPE1(unsigned) \
    SERIAL_ENUMERATE_STD_TYPE1(long) \
    SERIAL_ENUMERATE_STD_TYPE(unsigned long, ulong) \
    SERIAL_ENUMERATE_STD_TYPE1(Int8) \
    SERIAL_ENUMERATE_STD_TYPE1(Uint8)
#else
#define SERIAL_ENUMERATE_ALL_INTEGRAL_TYPES \
    SERIAL_ENUMERATE_STD_TYPE1(short) \
    SERIAL_ENUMERATE_STD_TYPE(unsigned short, ushort) \
    SERIAL_ENUMERATE_STD_TYPE1(int) \
    SERIAL_ENUMERATE_STD_TYPE1(unsigned) \
    SERIAL_ENUMERATE_STD_TYPE1(Int8) \
    SERIAL_ENUMERATE_STD_TYPE1(Uint8)
#endif

#define SERIAL_ENUMERATE_ALL_FLOAT_TYPES \
    SERIAL_ENUMERATE_STD_TYPE1(float) \
    SERIAL_ENUMERATE_STD_TYPE1(double)

#define SERIAL_ENUMERATE_ALL_STD_TYPES \
    SERIAL_ENUMERATE_STD_TYPE1(bool) \
    SERIAL_ENUMERATE_ALL_CHAR_TYPES \
    SERIAL_ENUMERATE_ALL_INTEGRAL_TYPES \
    SERIAL_ENUMERATE_ALL_FLOAT_TYPES

class NCBI_XSERIAL_EXPORT CPrimitiveTypeInfoBool : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef bool TObjectType;

    CPrimitiveTypeInfoBool(void);

    bool GetValueBool(TConstObjectPtr object) const;
    void SetValueBool(TObjectPtr object, bool value) const;
};

class NCBI_XSERIAL_EXPORT CPrimitiveTypeInfoChar : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef char TObjectType;

    CPrimitiveTypeInfoChar(void);

    char GetValueChar(TConstObjectPtr object) const;
    void SetValueChar(TObjectPtr object, char value) const;
    void GetValueString(TConstObjectPtr object, string& value) const;
    void SetValueString(TObjectPtr object, const string& value) const;
};

class NCBI_XSERIAL_EXPORT CPrimitiveTypeInfoInt : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef Int4 (*TGetInt4Function)(TConstObjectPtr objectPtr);
    typedef Uint4 (*TGetUint4Function)(TConstObjectPtr objectPtr);
    typedef void (*TSetInt4Function)(TObjectPtr objectPtr, Int4 v);
    typedef void (*TSetUint4Function)(TObjectPtr objectPtr, Uint4 v);
    typedef Int8 (*TGetInt8Function)(TConstObjectPtr objectPtr);
    typedef Uint8 (*TGetUint8Function)(TConstObjectPtr objectPtr);
    typedef void (*TSetInt8Function)(TObjectPtr objectPtr, Int8 v);
    typedef void (*TSetUint8Function)(TObjectPtr objectPtr, Uint8 v);

    CPrimitiveTypeInfoInt(size_t size, bool isSigned);

    void SetInt4Functions(TGetInt4Function, TSetInt4Function,
                          TGetUint4Function, TSetUint4Function);
    void SetInt8Functions(TGetInt8Function, TSetInt8Function,
                          TGetUint8Function, TSetUint8Function);

    Int4 GetValueInt4(TConstObjectPtr objectPtr) const;
    Uint4 GetValueUint4(TConstObjectPtr objectPtr) const;
    void SetValueInt4(TObjectPtr objectPtr, Int4 value) const;
    void SetValueUint4(TObjectPtr objectPtr, Uint4 value) const;
    Int8 GetValueInt8(TConstObjectPtr objectPtr) const;
    Uint8 GetValueUint8(TConstObjectPtr objectPtr) const;
    void SetValueInt8(TObjectPtr objectPtr, Int8 value) const;
    void SetValueUint8(TObjectPtr objectPtr, Uint8 value) const;
    
protected:
    TGetInt4Function m_GetInt4;
    TSetInt4Function m_SetInt4;
    TGetUint4Function m_GetUint4;
    TSetUint4Function m_SetUint4;
    TGetInt8Function m_GetInt8;
    TSetInt8Function m_SetInt8;
    TGetUint8Function m_GetUint8;
    TSetUint8Function m_SetUint8;
};

class NCBI_XSERIAL_EXPORT CPrimitiveTypeInfoDouble : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef double TObjectType;

    CPrimitiveTypeInfoDouble(void);

    double GetValueDouble(TConstObjectPtr objectPtr) const;
    void SetValueDouble(TObjectPtr objectPtr, double value) const;
};

class NCBI_XSERIAL_EXPORT CPrimitiveTypeInfoFloat : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef float TObjectType;

    CPrimitiveTypeInfoFloat(void);

    double GetValueDouble(TConstObjectPtr objectPtr) const;
    void SetValueDouble(TObjectPtr objectPtr, double value) const;
};

#if SIZEOF_LONG_DOUBLE != 0
class NCBI_XSERIAL_EXPORT CPrimitiveTypeInfoLongDouble : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef long double TObjectType;

    CPrimitiveTypeInfoLongDouble(void);

    double GetValueDouble(TConstObjectPtr objectPtr) const;
    void SetValueDouble(TObjectPtr objectPtr, double value) const;

    virtual long double GetValueLDouble(TConstObjectPtr objectPtr) const;
    virtual void SetValueLDouble(TObjectPtr objectPtr,
                                 long double value) const;
};
#endif

// CTypeInfo for C++ STL type string
class NCBI_XSERIAL_EXPORT CPrimitiveTypeInfoString : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    enum EType {
        eStringTypeVisible,
        eStringTypeUTF8
    };
    typedef string TObjectType;

    CPrimitiveTypeInfoString(EType type = eStringTypeVisible);

    char GetValueChar(TConstObjectPtr objectPtr) const;
    void SetValueChar(TObjectPtr objectPtr, char value) const;
    void GetValueString(TConstObjectPtr objectPtr, string& value) const;
    void SetValueString(TObjectPtr objectPtr, const string& value) const;
    EType GetStringType(void) const
    {
        return m_Type;
    }
    bool IsStringStore(void) const;
private:
    EType m_Type;
};

template<typename T>
class CPrimitiveTypeInfoCharPtr : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef T TObjectType;

    CPrimitiveTypeInfoCharPtr(void);

    char GetValueChar(TConstObjectPtr objectPtr) const;
    void SetValueChar(TObjectPtr objectPtr, char value) const;
    void GetValueString(TConstObjectPtr objectPtr, string& value) const;
    void SetValueString(TObjectPtr objectPtr, const string& value) const;
};

template<typename Char>
class CCharVectorTypeInfo : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef vector<Char> TObjectType;
    typedef Char TChar;

    CCharVectorTypeInfo(void);

    void GetValueString(TConstObjectPtr objectPtr, string& value) const;
    void SetValueString(TObjectPtr objectPtr, const string& value) const;
    void GetValueOctetString(TConstObjectPtr objectPtr,
                             vector<char>& value) const;
    void SetValueOctetString(TObjectPtr objectPtr,
                             const vector<char>& value) const;
};

class NCBI_XSERIAL_EXPORT CPrimitiveTypeInfoAnyContent
    : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    CPrimitiveTypeInfoAnyContent(void);

    void GetValueAnyContent(TConstObjectPtr objectPtr,
                            CAnyContentObject& value) const;
    void SetValueAnyContent(TObjectPtr objectPtr,
                            const CAnyContentObject& value) const;
};

class NCBI_XSERIAL_EXPORT CPrimitiveTypeInfoBitString
    : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    CPrimitiveTypeInfoBitString(void);

    virtual void GetValueBitString(TConstObjectPtr objectPtr,
                                   CBitString& value) const;
    virtual void SetValueBitString(TObjectPtr objectPtr,
                                   const CBitString& value) const;
};

/* @} */


END_NCBI_SCOPE

#endif  /* STDTYPESIMPL__HPP */
