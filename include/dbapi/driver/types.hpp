#ifndef DBAPI_DRIVER___TYPES__HPP
#define DBAPI_DRIVER___TYPES__HPP

/* $Id$
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
 * Author:  Vladimir Soussov
 *
 * File Description:  DB types
 *
 */


#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_limits.h>


BEGIN_NCBI_SCOPE


// Set of supported types
//

enum EDB_Type {
    eDB_Int,
    eDB_SmallInt,
    eDB_TinyInt,
    eDB_BigInt,
    eDB_VarChar,
    eDB_Char,
    eDB_VarBinary,
    eDB_Binary,
    eDB_Float,
    eDB_Double,
    eDB_DateTime,
    eDB_SmallDateTime,
    eDB_Text,
    eDB_Image,
    eDB_Bit,
    eDB_Numeric,

    eDB_UnsupportedType
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDB_Object::
//
// Base class for all "type objects" to support database NULL value
// and provide the means to get the type and to clone the object.
//

class CDB_Object
{
public:
    CDB_Object(bool is_null = true) : m_Null(is_null)  { return; }
    virtual ~CDB_Object();

    bool IsNULL() const  { return m_Null; }
    virtual void AssignNULL();

    virtual EDB_Type    GetType() const = 0;
    virtual CDB_Object* Clone()   const = 0;

protected:
    bool m_Null;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDB_Int::
//  CDB_SmallInt::
//  CDB_TinyInt::
//  CDB_BigInt::
//  CDB_VarChar::
//  CDB_Char::
//  CDB_VarBinary::
//  CDB_Binary::
//  CDB_Float::
//  CDB_Double::
//  CDB_Stream::
//  CDB_Image::
//  CDB_Text::
//  CDB_SmallDateTime::
//  CDB_DateTime::
//  CDB_Bit::
//  CDB_Numeric::
//
// Classes to represent objects of different types (derived from CDB_Object::)
//


class CDB_Int : public CDB_Object
{
public:
    CDB_Int()              : CDB_Object(true)             { return; }
    CDB_Int(const Int4& i) : CDB_Object(false), m_Val(i)  { return; }

    CDB_Int& operator= (const Int4& i) {
        m_Null = false;
        m_Val  = i;
        return *this;
    }

    Int4  Value()   const  { return m_Null ? 0 : m_Val; }
    void* BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone() const;

protected:
    Int4 m_Val;
};



class CDB_SmallInt : public CDB_Object
{
public:
    CDB_SmallInt()              : CDB_Object(true)             { return; }
    CDB_SmallInt(const Int2& i) : CDB_Object(false), m_Val(i)  { return; }

    CDB_SmallInt& operator= (const Int2& i) {
        m_Null = false;
        m_Val = i;
        return *this;
    }

    Int2  Value()   const  { return m_Null ? 0 : m_Val; }
    void* BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone() const;

protected:
    Int2 m_Val;
};



class CDB_TinyInt : public CDB_Object
{
public:
    CDB_TinyInt()               : CDB_Object(true)             { return; }
    CDB_TinyInt(const Uint1& i) : CDB_Object(false), m_Val(i)  { return; }

    CDB_TinyInt& operator= (const Uint1& i) {
        m_Null = false;
        m_Val = i;
        return *this;
    }

    Uint1 Value()   const  { return m_Null ? 0 : m_Val; }
    void* BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

protected:
    Uint1 m_Val;
};



class CDB_BigInt : public CDB_Object
{
public:
    CDB_BigInt()              : CDB_Object(true)             { return; }
    CDB_BigInt(const Int8& i) : CDB_Object(false), m_Val(i)  { return; }

    CDB_BigInt& operator= (const Int8& i) {
        m_Null = false;
        m_Val = i;
        return *this;
    }

    Int8 Value() const  { return m_Null ? 0 : m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

protected:
    Int8 m_Val;
};


class CDB_VarChar : public CDB_Object
{
public:
    // constructors
    CDB_VarChar()                           : CDB_Object(true)  { return; }
    CDB_VarChar(const string& s)            { SetValue(s); }
    CDB_VarChar(const char*   s)            { SetValue(s); }
    CDB_VarChar(const char*   s, size_t l)  { SetValue(s, l); }

    // assignment operators
    CDB_VarChar& operator= (const string& s)  { return SetValue(s); }
    CDB_VarChar& operator= (const char*   s)  { return SetValue(s); }

    // set-value methods
    CDB_VarChar& SetValue(const string& s) {
        m_Null = false;
        m_Size = s.copy(m_Val, sizeof(m_Val) - 1);
        m_Val[m_Size] = '\0';
        return *this;
    }
    CDB_VarChar& SetValue(const char* s) {
        if ( s ) {
            for (m_Size = 0;  (m_Size < sizeof(m_Val) - 1)  &&  (*s != '\0');
                 ++s) {
                m_Val[m_Size++] = *s;
            }
            m_Val[m_Size] = '\0';
            m_Null = false;
        }
        else {
            m_Null = true;
        }
        return *this;
    }
    CDB_VarChar& SetValue(const char* s, size_t l) {
        if ( s ) {
            m_Size = l < sizeof(m_Val) ? l : sizeof(m_Val) - 1;
            if ( m_Size ) {
                memcpy(m_Val, s, m_Size);
            }
            m_Val[m_Size] = '\0';
            m_Null = false;
        }
        else {
            m_Null = true;
        }
        return *this;
    }

    //
    const char* Value() const  { return m_Null ? 0 : m_Val;  }
    size_t      Size()  const  { return m_Null ? 0 : m_Size; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

protected:
    size_t m_Size;
    char   m_Val[256];
};



class CDB_Char : public CDB_Object
{
public:
    // static const size_t kMaxCharSize = 255; <<=not compatible with MS compiler
    enum {kMaxCharSize = 255 };

    CDB_Char(size_t s = 1) : CDB_Object(true) {
        m_Size = (s < 1) ? 1 : (s > kMaxCharSize ? kMaxCharSize : s);
        m_Val  = new char[m_Size + 1];
        memset(m_Val, ' ', m_Size);
        m_Val[m_Size] = '\0';
    }

    CDB_Char(size_t s, const string& v) :  CDB_Object(false) {
        m_Size = (s < 1) ? 1 : (s > kMaxCharSize ? kMaxCharSize : s);
        m_Val = new char[m_Size + 1];
        size_t l = v.copy(m_Val, m_Size);
        if (l < m_Size) {
            memset(m_Val + l, ' ', m_Size - l);
        }
        m_Val[m_Size] = '\0';
    }

    CDB_Char(size_t len, const char* str) :  CDB_Object(str == 0) {
        m_Size = (len < 1) ? 1 : (len > kMaxCharSize ? kMaxCharSize : len);
        m_Val = new char[m_Size + 1];

        if ( str ) {
            size_t l;
            for (l = 0;  (l < m_Size)  &&  (*str != '\0');  ++str) {
                m_Val[l++] = *str;
            }
            if (l < m_Size) {
                memset(m_Val + l, ' ', m_Size - l);
            }
        } else {
            memset(m_Val, ' ', m_Size);
        }

        m_Val[m_Size] = '\0';
    }

    CDB_Char(const CDB_Char& v) {
        m_Null = v.m_Null;
        m_Size = v.m_Size;
        m_Val = new char[m_Size + 1];
        memcpy(m_Val, v.m_Val, m_Size + 1);
    }


    CDB_Char& operator= (const CDB_Char& v) {
        m_Null = v.m_Null;
        size_t l = (m_Size > v.m_Size) ? v.m_Size : m_Size;
        memmove(m_Val, v.m_Val, l);
        if (l < m_Size)
            memset(m_Val + l, ' ', m_Size - l);
        return *this;
    }

    CDB_Char& operator= (const string& v) {
        m_Null = false;
        size_t l = v.copy(m_Val, m_Size);
        if (l < m_Size)
            memset(m_Val + l, ' ', m_Size - l);
        return *this;
    }

    CDB_Char& operator= (const char* v) {
        if (v == 0) {
            m_Null = true;
        }
        else {
            m_Null = false;
            size_t l;

            for (l = 0;  (l < m_Size)  &&  (*v != '\0');  ++v) {
                m_Val[l++] = *v;
            }
            if (l < m_Size)
                memset(m_Val + l, ' ', m_Size - l);
        }
        return *this;
    }

    void SetValue(const char* str, size_t len) {
        if ( str ) {
            if (len >= m_Size) {
                memcpy(m_Val, str, m_Size);
            }
            else {
                if ( len ) {
                    memcpy(m_Val, str, len);
                }
                memset(m_Val + len, ' ', m_Size - len);
            }
            m_Null = false;
        }
        else {
            m_Null = true;
        }
    }

    const char* Value() const  { return m_Null ? 0 : m_Val; }
    size_t      Size()  const  { return m_Size; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

    virtual ~CDB_Char();

protected:
    size_t m_Size;
    char*  m_Val;
};



class CDB_VarBinary : public CDB_Object
{
public:
    CDB_VarBinary()                         : CDB_Object(true)  { return; }
    CDB_VarBinary(const void* v, size_t l)  { SetValue(v, l); }

    void SetValue(const void* v, size_t l) {
        if (v  &&  l) {
            m_Size = l > sizeof(m_Val) ? sizeof(m_Val) : l;
            memcpy(m_Val, v, m_Size);
            m_Null = false;
        }
        else {
            m_Null = true;
        }
    }

    //
    const void* Value() const  { return m_Null ? 0 : (void*) m_Val; }
    size_t      Size()  const  { return m_Null ? 0 : m_Size; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

protected:
    size_t        m_Size;
    unsigned char m_Val[255];
};



class CDB_Binary : public CDB_Object
{
public:
    // static const size_t kMaxBinSize = 255; <<=not compatible with MS compiler
    enum { kMaxBinSize = 255 };

    CDB_Binary(size_t s = 1) : CDB_Object(true) {
        m_Size = (s < 1) ? 1 : (s > kMaxBinSize ? kMaxBinSize : s);
        m_Val = new unsigned char[m_Size];
        memset(m_Val, 0, m_Size);
    }

    CDB_Binary(size_t s, const void* v, size_t v_size) {
        m_Size = (s == 0) ? 1 : (s > kMaxBinSize ? kMaxBinSize : s);
        m_Val  = new unsigned char[m_Size];
        SetValue(v, v_size);
    }

    CDB_Binary(const CDB_Binary& v) {
        m_Null = v.m_Null;
        m_Size = v.m_Size;
        m_Val = new unsigned char[m_Size];
        memcpy(m_Val, v.m_Val, m_Size);
    }

    void SetValue(const void* v, size_t v_size) {
        if (v  &&  v_size) {
            memcpy(m_Val, v, (v_size > m_Size) ? m_Size : v_size);
            if (v_size < m_Size) {
                memset(m_Val + v_size, 0, m_Size - v_size);
            }
            m_Null = false;
        } else {
            m_Null = true;
        }
    }

    CDB_Binary& operator= (const CDB_Binary& v) {
        m_Null = v.m_Null;
        size_t l = (m_Size > v.m_Size) ? v.m_Size : m_Size;
        memmove(m_Val, v.m_Val, l);
        if (l < m_Size) {
            memset(m_Val+l, 0, m_Size - l);
        }
        return *this;
    }

    //
    const void* Value() const  { return m_Null ? 0 : (void*) m_Val; }
    size_t      Size()  const  { return m_Size; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

    virtual ~CDB_Binary();

protected:
    size_t         m_Size;
    unsigned char* m_Val;
};



class CDB_Float : public CDB_Object
{
public:
    CDB_Float()        : CDB_Object(true)             { return; }
    CDB_Float(float i) : CDB_Object(false), m_Val(i)  { return; }

    CDB_Float& operator= (const float& i) {
        m_Null = false;
        m_Val  = i;
        return *this;
    }

    float Value()   const { return m_Null ? 0 : m_Val; }
    void* BindVal() const { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

protected:
    float m_Val;
};



class CDB_Double : public CDB_Object
{
public:
    CDB_Double()         : CDB_Object(true)             { return; }
    CDB_Double(double i) : CDB_Object(false), m_Val(i)  { return; }

    CDB_Double& operator= (const double& i) {
        m_Null = false;
        m_Val  = i;
        return *this;
    }

    //
    double Value()   const  { return m_Null ? 0 : m_Val; }
    void*  BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

protected:
    double m_Val;
};


class CMemStore;


class CDB_Stream : public CDB_Object
{
public:
    // assignment
    virtual void AssignNULL();
    CDB_Stream&  Assign(const CDB_Stream& v);

    // data manipulations
    virtual size_t Read     (void* buff, size_t nof_bytes);
    virtual size_t Append   (const void* buff, size_t nof_bytes);
    virtual void   Truncate (size_t nof_bytes = kMax_UInt);
    virtual bool   MoveTo   (size_t byte_number);

    // current size of data
    virtual size_t Size() const;

protected:
    // 'ctors
    CDB_Stream();
    virtual ~CDB_Stream();

private:
    // data storage
    CMemStore* m_Store;
};



class CDB_Image : public CDB_Stream
{
public:
    CDB_Image& operator= (const CDB_Image& image);

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
};



class CDB_Text : public CDB_Stream
{
public:
    virtual size_t Append(const void* buff, size_t nof_bytes = 0/*strlen*/);
    virtual size_t Append(const string& s);

    CDB_Text& operator= (const CDB_Text& text);

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
};



class CDB_SmallDateTime : public CDB_Object
{
public:
    CDB_SmallDateTime() : m_NCBITime(CTime::eCurrent) {
        m_Status = 0x1;
    }
    CDB_SmallDateTime(const CTime& t) {
        m_NCBITime = t;
        m_Status   = 0x1;
	m_Null     = false;
    }
    CDB_SmallDateTime(Uint2 days, Uint2 minutes) {
        m_DBTime.days = days;
        m_DBTime.time = minutes;
        m_Status      = 0x2;
	m_Null        = false;
    }

    CDB_SmallDateTime& Assign(Uint2 days, Uint2 minutes) {
        m_DBTime.days = days;
        m_DBTime.time = minutes;
        m_Status      = 0x2;
        m_Null        = false;
        return *this;
    }

    CDB_SmallDateTime& operator= (const CTime& t) {
        m_NCBITime = t;
        m_Status = 0x1;
	m_Null= false;
        return *this;
    }

    const CTime& Value() const {
        if((m_Status & 0x1) == 0) {
            m_NCBITime.SetTimeDBU(m_DBTime);
            m_Status |= 0x1;
        }
        return m_NCBITime;
    }

    Uint2 GetDays() const {
        if((m_Status & 0x2) == 0) {
            m_DBTime = m_NCBITime.GetTimeDBU();
            m_Status |= 0x2;
        }
        return m_DBTime.days;
    }

    Uint2 GetMinutes() const {
        if((m_Status & 0x2) == 0) {
            m_DBTime = m_NCBITime.GetTimeDBU();
            m_Status |= 0x2;
        }
        return m_DBTime.time;
    }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

protected:
    mutable CTime        m_NCBITime;
    mutable TDBTimeU     m_DBTime;
    // which of m_NCBITime(0x1), m_DBTime(0x2) is valid;  they both can be valid
    mutable unsigned int m_Status;
};



class CDB_DateTime : public CDB_Object
{
public:
    CDB_DateTime() : m_NCBITime(CTime::eCurrent) {
        m_Status = 0x1;
    }
    CDB_DateTime(const CTime& t) {
        m_NCBITime = t;
        m_Status = 0x1;
	m_Null= false;
    }
    CDB_DateTime(Int4 d, Int4 s300) {
        m_DBTime.days = d;
        m_DBTime.time = s300;
        m_Status = 0x2;
	m_Null= false;
    }

    CDB_DateTime& operator= (const CTime& t) {
        m_NCBITime = t;
        m_Status = 0x1;
	m_Null= false;
        return *this;
    }

    CDB_DateTime& Assign(Int4 d, Int4 s300) {
        m_DBTime.days = d;
        m_DBTime.time = s300;
        m_Status = 0x2;
        m_Null = false;
        return *this;
    }

    const CTime& Value() const {
        if((m_Status & 0x1) == 0) {
            m_NCBITime.SetTimeDBI(m_DBTime);
            m_Status |= 0x1;
        }
        return m_NCBITime;
    }

    Int4 GetDays() const {
        if((m_Status & 0x2) == 0) {
            m_DBTime = m_NCBITime.GetTimeDBI();
            m_Status |= 0x2;
        }
        return m_DBTime.days;
    }

    Int4 Get300Secs() const {
        if((m_Status & 0x2) == 0) {
            m_DBTime = m_NCBITime.GetTimeDBI();
            m_Status |= 0x2;
        }
        return m_DBTime.time;
    }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

protected:
    mutable CTime        m_NCBITime;
    mutable TDBTimeI     m_DBTime;
    // which of m_NCBITime(0x1), m_DBTime(0x2) is valid;  they both can be valid
    mutable unsigned int m_Status;
};



class CDB_Bit : public CDB_Object
{
public:
    CDB_Bit()       : CDB_Object(true)   { return; }
    CDB_Bit(int  i) : CDB_Object(false)  { m_Val = i ? 1 : 0; }
    CDB_Bit(bool i) : CDB_Object(false)  { m_Val = i ? 1 : 0; }

    CDB_Bit& operator= (const int& i) {
        m_Null = false;
        m_Val = i ? 1 : 0;
        return *this;
    }

    CDB_Bit& operator= (const bool& i) {
        m_Null = false;
        m_Val = i ? 1 : 0;
        return *this;
    }

    int   Value()   const  { return m_Null ? 0 : (int) m_Val; }
    void* BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

protected:
    Uint1 m_Val;
};



class CDB_Numeric : public CDB_Object
{
public:
    CDB_Numeric() : CDB_Object(true)  { return; }
    CDB_Numeric(unsigned int precision, unsigned int scale)
        : CDB_Object(false) {
        m_Precision = precision;
        m_Scale     = scale;
        memset(m_Body, 0, sizeof(m_Body));
    }
    CDB_Numeric(unsigned int precision, unsigned int scale,
                const unsigned char* arr)
        : CDB_Object(false) {
        m_Precision = precision;
        m_Scale     = scale;
        memcpy(m_Body, arr, sizeof(m_Body));
    }

    CDB_Numeric(unsigned int precision, unsigned int scale, const char* val) {
        x_MakeFromString(precision, scale, val);
    }
    CDB_Numeric(unsigned int precision, unsigned int scale, const string& val) {
        x_MakeFromString(precision, scale, val.c_str());
    }

    CDB_Numeric& Assign(unsigned int precision, unsigned int scale,
                        const unsigned char* arr) {
        m_Precision = precision;
        m_Scale     = scale;
        m_Null      = false;
        memcpy(m_Body, arr, sizeof(m_Body));
        return *this;
    }
 
    CDB_Numeric& operator= (const char* val) {
        x_MakeFromString(m_Precision, m_Scale, val);
        return *this;
    }
    CDB_Numeric& operator= (const string& val) {
        x_MakeFromString(m_Precision, m_Scale, val.c_str());
        return *this;
    }

    string Value() const;

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

protected:
    void x_MakeFromString(unsigned int precision,
                          unsigned int scale,
                          const char*  val);
    Uint1         m_Precision;
    Uint1         m_Scale;
    unsigned char m_Body[34];
};


END_NCBI_SCOPE

#endif  /* DBAPI_DRIVER___TYPES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2001/12/28 21:22:39  sapojnik
 * Made compatible with MS compiler: long long to Int8, static const within class def to enum
 *
 * Revision 1.3  2001/12/14 17:58:26  soussov
 * fixes bug in datetime related constructors
 *
 * Revision 1.2  2001/11/06 17:58:03  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/09/21 23:39:52  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
