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

template<typename T>
class CPrimitiveTypeFunctions
{
public:
    static void Read(CObjectIStream& in,
                     TTypeInfo , TObjectPtr objectPtr)
        {
            in.ReadStd(CTypeConverter<T>::Get(objectPtr));
        }
    static void Write(CObjectOStream& out,
                      TTypeInfo , TConstObjectPtr objectPtr)
        {
            out.WriteStd(CTypeConverter<T>::Get(objectPtr));
        }
    static void Skip(CObjectIStream& in, TTypeInfo )
        {
            T data;
            in.ReadStd(data);
        }
    static void Copy(CObjectStreamCopier& copier, TTypeInfo )
        {
            T data;
            copier.In().ReadStd(data);
            copier.Out().WriteStd(data);
        }
    static TObjectPtr Create(TTypeInfo )
        {
            return new T();
        }
    static TObjectPtr CreateZero(TTypeInfo )
        {
            return new T(0);
        }
};

// template to standard C types with default value 0 (int, double, char* etc.)
template<typename T>
class CPrimitiveTypeInfoImpl : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef T TObjectType;
    CPrimitiveTypeInfoImpl(EPrimitiveValueType valueType)
        : CParent(sizeof(TObjectType), valueType)
        {
            SetCreateFunction(&CPrimitiveTypeFunctions<T>::CreateZero);
            SetReadFunction(&CPrimitiveTypeFunctions<T>::Read);
            SetWriteFunction(&CPrimitiveTypeFunctions<T>::Write);
            SetSkipFunction(&CPrimitiveTypeFunctions<T>::Skip);
            SetCopyFunction(&CPrimitiveTypeFunctions<T>::Copy);
        }

    static TObjectType& Get(TObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }

    virtual bool IsDefault(TConstObjectPtr object) const
        {
            return Get(object) == 0;
        }

    virtual void SetDefault(TObjectPtr dst) const
        {
            Get(dst) = 0;
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            return Get(object1) == Get(object2);
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            Get(dst) = Get(src);
        }
};

class CPrimitiveTypeInfoBool : public CPrimitiveTypeInfoImpl<bool>
{
    typedef CPrimitiveTypeInfoImpl<bool> CParent;
public:
    CPrimitiveTypeInfoBool(void)
        : CParent(ePrimitiveValueBool)
        {
        }

    virtual bool GetValueBool(TConstObjectPtr object) const;
    virtual void SetValueBool(TObjectPtr object, bool value) const;
};

class CPrimitiveTypeInfoChar : public CPrimitiveTypeInfoImpl<char>
{
    typedef CPrimitiveTypeInfoImpl<char> CParent;
public:
    CPrimitiveTypeInfoChar(void)
        : CParent(ePrimitiveValueChar)
        {
        }

    virtual char GetValueChar(TConstObjectPtr object) const;
    virtual void SetValueChar(TObjectPtr object, char value) const;
};

template<typename T>
class CPrimitiveTypeInfoLong : public CPrimitiveTypeInfoImpl<T>
{
    typedef CPrimitiveTypeInfoImpl<T> CParent;

    static bool x_IsSigned(void)
        {
            return TObjectType(-1) < 0;
        }
    static void x_CheckNoSign(long value)
        {
            if ( value < 0 ) // value doesn't fit
                ThrowIntegerOverflow();
        }
public:
    CPrimitiveTypeInfoLong(void)
        : CParent(ePrimitiveValueInteger)
        {
        }
    virtual bool IsSigned(void) const
        {
            return x_IsSigned();
        }
    virtual long GetValueLong(TConstObjectPtr objectPtr) const
        {
            TObjectType value = Get(objectPtr);
            if ( sizeof(TObjectType) == sizeof(long) && !x_IsSigned() ) {
                // type is unsigned long
                x_CheckNoSign(value);
            }
            return value;
        }
    virtual unsigned long GetValueULong(TConstObjectPtr objectPtr) const
        {
            TObjectType value = Get(objectPtr);
            if ( sizeof(TObjectType) == sizeof(long) && x_IsSigned() ) {
                // type is signed long
                x_CheckNoSign(value);
            }
            return value;
        }
    virtual void SetValueLong(TObjectPtr objectPtr, long value) const
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
    virtual void SetValueULong(TObjectPtr objectPtr, unsigned long value) const
        {
            TObjectType newValue = TObjectType(value);
            if ( sizeof(TObjectType) == sizeof(long) ) {
                if ( x_IsSigned() ) {
                    // type is signed long
                    x_CheckNoSign(newValue);
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

template<typename T>
class CPrimitiveTypeInfoDouble : public CPrimitiveTypeInfoImpl<T>
{
    typedef CPrimitiveTypeInfoImpl<T> CParent;
public:
    typedef T TObjectType;

    CPrimitiveTypeInfoDouble(void)
        : CParent(ePrimitiveValueReal)
        {
        }
    virtual double GetValueDouble(TConstObjectPtr objectPtr) const
        {
            return Get(objectPtr);
        }
    virtual void SetValueDouble(TObjectPtr objectPtr, double value) const
        {
            Get(objectPtr) = value;
        }
};

// CTypeInfo for C++ STL type string
class CPrimitiveTypeInfoString : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    typedef string TObjectType;

    CPrimitiveTypeInfoString(void);

    static TObjectType& Get(TObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr , TConstObjectPtr ) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

    virtual void GetValueString(TConstObjectPtr objectPtr,
                                string& value) const;
    virtual void SetValueString(TObjectPtr objectPtr,
                                const string& value) const;
};

template<typename T>
class CPrimitiveTypeInfoCharPtr : public CPrimitiveTypeInfoImpl<T>
{
    typedef CPrimitiveTypeInfoImpl<T> CParent;
public:
    CPrimitiveTypeInfoCharPtr(void)
        : CParent(ePrimitiveValueString)
        {
        }

    static void Reset(TObjectPtr dst)
        {
            free(const_cast<char*>(Get(dst)));
            Get(dst) = 0;
        }

    virtual void SetDefault(TObjectPtr dst) const
        {
            Reset(dst);
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            return strcmp(Get(object1), Get(object2)) == 0;
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            TObjectType value = Get(src);
            _ASSERT(Get(dst) != value);
            Reset(dst);
            if ( value )
                Get(dst) = NotNull(strdup(value));
        }

    virtual void GetValueString(TConstObjectPtr objectPtr,
                                string& value) const
        {
            value = Get(objectPtr);
        }
    virtual void SetValueString(TObjectPtr objectPtr,
                                const string& value) const
        {
            Get(objectPtr) = NotNull(strdup(value.c_str()));
        }
};

class CStringStoreTypeInfo : public CPrimitiveTypeInfoString
{
public:
    CStringStoreTypeInfo(void);
};

class CNullBoolTypeInfo : public CPrimitiveTypeInfoBool
{
public:
    CNullBoolTypeInfo(void);
};

class COctetStringTypeInfoBase : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    COctetStringTypeInfoBase(size_t size);

private:
    static void SkipByteBlock(CObjectIStream& in, TTypeInfo objectType);
    static void CopyByteBlock(CObjectStreamCopier& copier,
                              TTypeInfo objectType);
};

template<typename Char>
class CCharVectorFunctions
{
public:
    typedef vector<Char> TObjectType;
    typedef Char TChar;

    static char* ToChar(Char* p)
        { return reinterpret_cast<char*>(p); }
    static const char* ToChar(const Char* p)
        { return reinterpret_cast<const char*>(p); }
    static const Char* ToTChar(const char* p)
        { return reinterpret_cast<const Char*>(p); }

    static void Read(CObjectIStream& in,
                     TTypeInfo , TObjectPtr objectPtr)
        {
            TObjectType& o = CTypeConverter<TObjectType>::Get(objectPtr);
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
            const TObjectType& o = CTypeConverter<TObjectType>::Get(objectPtr);
            size_t length = o.size();
            CObjectOStream::ByteBlock block(out, length);
            if ( length > 0 )
                block.Write(ToChar(&o.front()), length);
        }
    static TObjectPtr Create(TTypeInfo )
        {
            return new TObjectType();
        }
};

template<typename Char>
class CCharVectorTypeInfoImpl : public COctetStringTypeInfoBase
{
    typedef COctetStringTypeInfoBase CParent;
public:
    typedef vector<Char> TObjectType;
    typedef Char TChar;

    CCharVectorTypeInfoImpl(void)
        : CParent(sizeof(TObjectType))
        {
            SetCreateFunction(&CCharVectorFunctions<Char>::Create);
            SetReadFunction(&CCharVectorFunctions<Char>::Read);
            SetWriteFunction(&CCharVectorFunctions<Char>::Write);
        }

private:
    // helper methods
    static TObjectType& Get(TObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return CTypeConverter<TObjectType>::Get(object);
        }

    static char* ToChar(Char* p)
        { return reinterpret_cast<char*>(p); }
    static const char* ToChar(const Char* p)
        { return reinterpret_cast<const char*>(p); }
    static const Char* ToTChar(const char* p)
        { return reinterpret_cast<const Char*>(p); }

public:
    virtual bool IsDefault(TConstObjectPtr object) const
        {
            return Get(object).empty();
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            return Get(object1) == Get(object2);
        }

    virtual void SetDefault(TObjectPtr dst) const
        {
            Get(dst).clear();
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            Get(dst) = Get(src);
        }

    virtual void GetValueOctetString(TConstObjectPtr objectPtr,
                                     vector<char>& value) const
        {
            const TObjectType& obj = Get(objectPtr);
            value.clear();
            const char* buffer = ToChar(&obj.front());
            value.insert(value.end(), buffer, buffer + obj.size());
        }
    virtual void SetValueOctetString(TObjectPtr objectPtr,
                                     const vector<char>& value) const
        {
            TObjectType& obj = Get(objectPtr);
            obj.clear();
            const Char* buffer = ToTChar(&value.front());
            obj.insert(obj.end(), buffer, buffer + value.size());
        }
};

//#include <serial/stdtypesimpl.inl>

END_NCBI_SCOPE

#endif  /* STDTYPESIMPL__HPP */
