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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2000/10/13 16:28:32  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.6  2000/09/19 20:16:53  vasilche
* Fixed type in CStlClassInfo_auto_ptr.
* Added missing include serialutil.hpp.
*
* Revision 1.5  2000/09/18 20:00:11  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.4  2000/09/01 13:16:03  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.3  2000/07/03 20:47:18  vasilche
* Removed unused variables/functions.
*
* Revision 1.2  2000/07/03 19:04:25  vasilche
* Fixed type references in templates.
*
* Revision 1.1  2000/07/03 18:42:37  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/stdtypes.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serialutil.hpp>

BEGIN_NCBI_SCOPE

// throw various exceptions
void ThrowIntegerOverflow(void);
void ThrowIncompatibleValue(void);
void ThrowIllegalCall(void);

#define SERIAL_ENUMERATE_STD_TYPE1(Type) SERIAL_ENUMERATE_STD_TYPE(Type, Type)

#define SERIAL_ENUMERATE_ALL_CHAR_TYPES \
    SERIAL_ENUMERATE_STD_TYPE1(char) \
    SERIAL_ENUMERATE_STD_TYPE(signed char, schar) \
    SERIAL_ENUMERATE_STD_TYPE(unsigned char, uchar)

#define SERIAL_ENUMERATE_ALL_INTEGRAL_TYPES \
    SERIAL_ENUMERATE_STD_TYPE1(short) \
    SERIAL_ENUMERATE_STD_TYPE(unsigned short, ushort) \
    SERIAL_ENUMERATE_STD_TYPE1(int) \
    SERIAL_ENUMERATE_STD_TYPE1(unsigned) \
    SERIAL_ENUMERATE_STD_TYPE1(long) \
    SERIAL_ENUMERATE_STD_TYPE(unsigned long, ulong)

#define SERIAL_ENUMERATE_ALL_FLOAT_TYPES \
    SERIAL_ENUMERATE_STD_TYPE1(float) \
    SERIAL_ENUMERATE_STD_TYPE1(double)


#define SERIAL_ENUMERATE_ALL_STD_TYPES \
    SERIAL_ENUMERATE_STD_TYPE1(bool) \
    SERIAL_ENUMERATE_ALL_CHAR_TYPES \
    SERIAL_ENUMERATE_ALL_INTEGRAL_TYPES \
    SERIAL_ENUMERATE_ALL_FLOAT_TYPES

class CPrimitiveTypeInfoBool : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef bool TObjectType;

    CPrimitiveTypeInfoBool(void);

    bool GetValueBool(TConstObjectPtr object) const;
    void SetValueBool(TObjectPtr object, bool value) const;
};

class CPrimitiveTypeInfoChar : public CPrimitiveTypeInfo
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

class CPrimitiveTypeInfoLong : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef long (*TGetLongFunction)(TConstObjectPtr objectPtr);
    typedef unsigned long (*TGetULongFunction)(TConstObjectPtr objectPtr);
    typedef void (*TSetLongFunction)(TObjectPtr objectPtr, long v);
    typedef void (*TSetULongFunction)(TObjectPtr objectPtr, unsigned long v);

    CPrimitiveTypeInfoLong(size_t size, bool isSigned);

    void SetLongFunctions(TGetLongFunction, TSetLongFunction,
                          TGetULongFunction, TSetULongFunction);

    long GetValueLong(TConstObjectPtr objectPtr) const;
    unsigned long GetValueULong(TConstObjectPtr objectPtr) const;
    void SetValueLong(TObjectPtr objectPtr, long value) const;
    void SetValueULong(TObjectPtr objectPtr, unsigned long value) const;
    
protected:
    TGetLongFunction m_GetLong;
    TSetLongFunction m_SetLong;
    TGetULongFunction m_GetULong;
    TSetULongFunction m_SetULong;
};

class CPrimitiveTypeInfoDouble : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef double TObjectType;

    CPrimitiveTypeInfoDouble(void);

    double GetValueDouble(TConstObjectPtr objectPtr) const;
    void SetValueDouble(TObjectPtr objectPtr, double value) const;
};

class CPrimitiveTypeInfoFloat : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef float TObjectType;

    CPrimitiveTypeInfoFloat(void);

    double GetValueDouble(TConstObjectPtr objectPtr) const;
    void SetValueDouble(TObjectPtr objectPtr, double value) const;
};

#if HAVE_LONG_DOUBLE
class CPrimitiveTypeInfoLongDouble : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef long double TObjectType;

    CPrimitiveTypeInfoLongDouble(void);

    double GetValueDouble(TConstObjectPtr objectPtr) const;
    void SetValueDouble(TObjectPtr objectPtr, double value) const;
};
#endif

// CTypeInfo for C++ STL type string
class CPrimitiveTypeInfoString : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef string TObjectType;

    CPrimitiveTypeInfoString(void);

    char GetValueChar(TConstObjectPtr objectPtr) const;
    void SetValueChar(TObjectPtr objectPtr, char value) const;
    void GetValueString(TConstObjectPtr objectPtr, string& value) const;
    void SetValueString(TObjectPtr objectPtr, const string& value) const;
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

//#include <serial/stdtypesimpl.inl>

END_NCBI_SCOPE

#endif  /* STDTYPESIMPL__HPP */
