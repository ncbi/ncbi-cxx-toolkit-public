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

// template to standard C types with default value 0 (int, double, char* etc.)
template<typename T>
class CPrimitiveTypeInfoImpl : public CPrimitiveTypeInfo
{
public:
    typedef T TObjectType;
    typedef CPrimitiveTypeInfo::EValueType EValueType;

    static TObjectType& Get(TObjectPtr object)
        {
            return CType<TObjectType>::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return CType<TObjectType>::Get(object);
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }
    virtual TObjectPtr Create(void) const
        {
            return new TObjectType(0);
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

protected:
    virtual void SkipData(CObjectIStream& in) const
        {
            in.SkipStd(TObjectType(0));
        }
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            in.ReadStd(Get(object));
        }
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            out.WriteStd(Get(object));
        }
};

class CPrimitiveTypeInfoBool : public CPrimitiveTypeInfoImpl<bool>
{
public:
    virtual EValueType GetValueType(void) const;

    virtual bool GetValueBool(TConstObjectPtr object) const;
    virtual void SetValueBool(TObjectPtr object, bool value) const;
};

class CPrimitiveTypeInfoChar : public CPrimitiveTypeInfoImpl<char>
{
public:
    virtual EValueType GetValueType(void) const;

    virtual char GetValueChar(TConstObjectPtr object) const;
    virtual void SetValueChar(TObjectPtr object, char value) const;
};

template<typename T>
class CPrimitiveTypeInfoLong : public CPrimitiveTypeInfoImpl<T>
{
    typedef CPrimitiveTypeInfo::EValueType EValueType;

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
    virtual EValueType GetValueType(void) const
        {
            return eInteger;
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
public:
    typedef T TObjectType;
    typedef CPrimitiveTypeInfo::EValueType EValueType;

    virtual EValueType GetValueType(void) const
        {
            return eReal;
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
public:
    typedef string TObjectType;

    virtual EValueType GetValueType(void) const;

    static TObjectType& Get(TObjectPtr object)
        {
            return CType<TObjectType>::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return CType<TObjectType>::Get(object);
        }

    virtual size_t GetSize(void) const;
    virtual TObjectPtr Create(void) const;
    
    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr , TConstObjectPtr ) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

    virtual void GetValueString(TConstObjectPtr objectPtr, string& value) const;
    virtual void SetValueString(TObjectPtr objectPtr, const string& value) const;

protected:
    virtual void SkipData(CObjectIStream& in) const;
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
};

template<typename T>
class CPrimitiveTypeInfoCharPtr : public CPrimitiveTypeInfoImpl<T>
{
public:
    typedef CPrimitiveTypeInfo::EValueType EValueType;

    virtual EValueType GetValueType(void) const
        {
            return eString;
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

    virtual void GetValueString(TConstObjectPtr objectPtr, string& value) const
        {
            value = Get(objectPtr);
        }
    virtual void SetValueString(TObjectPtr objectPtr, const string& value) const
        {
            Get(objectPtr) = NotNull(strdup(value.c_str()));
        }
};

class CStringStoreTypeInfo : public CPrimitiveTypeInfoString
{
protected:
    virtual void SkipData(CObjectIStream& in) const;
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
};

class CNullBoolTypeInfo : public CPrimitiveTypeInfoBool
{
protected:
    virtual void SkipData(CObjectIStream& in) const;
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
};

template<typename Char>
class CCharVectorTypeInfoImpl : public CPrimitiveTypeInfo
{
public:
    typedef vector<Char> TObjectType;
    typedef Char TChar;
    typedef CPrimitiveTypeInfo::EValueType EValueType;

    virtual EValueType GetValueType(void) const
        {
            return eOctetString;
        }

private:
    // helper methods
    static TObjectType& Get(TObjectPtr object)
        {
            return CType<TObjectType>::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return CType<TObjectType>::Get(object);
        }

    static char* ToChar(Char* p)
        { return reinterpret_cast<char*>(p); }
    static const char* ToChar(const Char* p)
        { return reinterpret_cast<const char*>(p); }
    static const Char* ToTChar(const char* p)
        { return reinterpret_cast<const Char*>(p); }

public:
    virtual size_t GetSize(void) const
        {
            return CType<TObjectType>::GetSize();
        }

    virtual TObjectPtr Create(void) const
        {
            return new TObjectType();
        }

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

protected:
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const TObjectType& o = Get(object);
            size_t length = o.size();
            CObjectOStream::ByteBlock block(out, length);
            if ( length > 0 )
                block.Write(ToChar(&o.front()), length);
        }
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TObjectType& o = Get(object);
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
    virtual void SkipData(CObjectIStream& in) const
        {
            in.SkipByteBlock();
        }
};

//#include <serial/stdtypesimpl.inl>

END_NCBI_SCOPE

#endif  /* STDTYPESIMPL__HPP */
