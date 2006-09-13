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


/** @addtogroup DbTypes
 *
 * @{
 */


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
    eDB_LongChar,
    eDB_LongBinary,

    eDB_UnsupportedType
};



/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CWString
{
public:
    CWString(void);
    CWString(const CWString& str);

    explicit CWString(const char* str,
                      string::size_type size = string::npos,
                      EEncoding enc = eEncoding_Unknown);
    explicit CWString(const wchar_t* str,
                      wstring::size_type size = wstring::npos);
    explicit CWString(const string& str,
                      EEncoding enc = eEncoding_Unknown);
    explicit CWString(const wstring& str);

    ~CWString(void);

    CWString& operator=(const CWString& str);

public:
    operator char*(void) const
    {
        if (!(GetAvailableValueType() & eChar)) {
            x_MakeString();
        }

        return const_cast<char*>(m_Char);
    }
    operator const char*(void) const
    {
        if (!(GetAvailableValueType() & eChar)) {
            x_MakeString();
        }

        return m_Char;
    }
    operator wchar_t*(void) const
    {
        if (!(GetAvailableValueType() & eWChar)) {
            x_MakeWString();
        }

        return const_cast<wchar_t*>(m_WChar);
    }
    operator const wchar_t*(void) const
    {
        if (!(GetAvailableValueType() & eWChar)) {
            x_MakeWString();
        }

        return m_WChar;
    }

public:
    // str_enc - expected string encoding.
    const string& AsLatin1(EEncoding str_enc = eEncoding_Unknown) const
    {
        if (!(GetAvailableValueType() & eString)) {
            x_MakeString(str_enc);
        }

        return m_String;
    }
    const string& AsUTF8(EEncoding str_enc = eEncoding_Unknown) const
    {
        if (!(GetAvailableValueType() & eUTF8String)) {
            x_MakeUTF8String(str_enc);
        }

        return m_UTF8String;
    }
    const wstring& AsUnicode(EEncoding str_enc = eEncoding_Unknown) const
    {
        if (!(GetAvailableValueType() & eWString)) {
            x_MakeWString(str_enc);
        }

        return m_WString;
    }
    size_t GetSymbolNum(void) const;

public:
    void Clear(void);
    void Assign(const char* str,
                string::size_type size = string::npos,
                EEncoding enc = eEncoding_Unknown);
    void Assign(const wchar_t* str,
                wstring::size_type size = wstring::npos);
    void Assign(const string& str,
                EEncoding enc = eEncoding_Unknown);
    void Assign(const wstring& str);

protected:
    int GetAvailableValueType(void) const
    {
        return m_AvailableValueType;
    }

    void x_MakeString(EEncoding str_enc = eEncoding_Unknown) const;
    void x_MakeWString(EEncoding str_enc = eEncoding_Unknown) const;
    void x_MakeUTF8String(EEncoding str_enc = eEncoding_Unknown) const;

    void x_CalculateEncoding(EEncoding str_enc) const;
    void x_UTF8ToString(EEncoding str_enc = eEncoding_Unknown) const;
    void x_StringToUTF8(EEncoding str_enc = eEncoding_Unknown) const;

protected:
    enum {eChar = 1, eWChar = 2, eString = 4, eWString = 8, eUTF8String = 16};

    mutable int             m_AvailableValueType;
    mutable EEncoding       m_StringEncoding; // Source string encoding.
    mutable const char*     m_Char;
    mutable const wchar_t*  m_WChar;
    mutable string          m_String;
    mutable wstring         m_WString;
    mutable CStringUTF8     m_UTF8String;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CDB_Object::
//
// Base class for all "type objects" to support database NULL value
// and provide the means to get the type and to clone the object.
//

class NCBI_DBAPIDRIVER_EXPORT CDB_Object
{
public:
    CDB_Object(bool is_null = true) : m_Null(is_null)  { return; }
    virtual ~CDB_Object();

    bool IsNULL() const  { return m_Null; }
    virtual void AssignNULL();

    virtual EDB_Type    GetType() const = 0;
    virtual CDB_Object* Clone()   const = 0;
    virtual void AssignValue(CDB_Object& v)= 0;

    // Create and return a new object (with internal value NULL) of type "type".
    // NOTE:  "size" matters only for eDB_Char, eDB_Binary, eDB_LongChar, eDB_LongBinary.
    static CDB_Object* Create(EDB_Type type, size_t size = 1);

    // Get human-readable type name for db_type
    static const char* GetTypeName(EDB_Type db_type);

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


class NCBI_DBAPIDRIVER_EXPORT CDB_Int : public CDB_Object
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
    virtual void AssignValue(CDB_Object& v);

protected:
    Int4 m_Val;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_SmallInt : public CDB_Object
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
    virtual void AssignValue(CDB_Object& v);

protected:
    Int2 m_Val;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_TinyInt : public CDB_Object
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
    virtual void AssignValue(CDB_Object& v);

protected:
    Uint1 m_Val;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_BigInt : public CDB_Object
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
    void* BindVal() const  { return (void*) &m_Val; }


    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(CDB_Object& v);

protected:
    Int8 m_Val;
};


// Abstract base class.
class NCBI_DBAPIDRIVER_EXPORT CDB_String : public CDB_Object
{
public:
    CDB_String(void);
    CDB_String(const CDB_String& other);
    explicit CDB_String(const string& s,
                        EEncoding enc = eEncoding_Unknown);
    explicit CDB_String(const char* s,
                        string::size_type size = string::npos,
                        EEncoding enc = eEncoding_Unknown);
    virtual ~CDB_String(void);

public:
    // assignment operators
    CDB_String& operator= (const CDB_String& other);
    CDB_String& operator= (const string& s);
    CDB_String& operator= (const char* s);

public:
#if defined(HAVE_WSTRING)
    // enc - expected source string encoding.
    const wchar_t*  AsUnicode(EEncoding enc) const
    {
        return m_Null ? NULL : m_WString.AsUnicode(enc).c_str();
    }
#endif

    const char* Value(void) const
    {
        return m_Null ? 0 : static_cast<const char*>(m_WString);
    }
    size_t Size(void) const
    {
        return m_Null ? 0 : m_WString.GetSymbolNum();
    }

public:
    // set-value methods
    void Assign(const CDB_String& other);
    void Assign(const string& s,
                EEncoding enc = eEncoding_Unknown);
    void Assign(const char* s,
                string::size_type size = string::npos,
                EEncoding enc = eEncoding_Unknown);

private:
    CWString m_WString;
};


class NCBI_DBAPIDRIVER_EXPORT CDB_VarChar : public CDB_String
{
public:
    // constructors
    CDB_VarChar();
    CDB_VarChar(const string& s,
                EEncoding enc = eEncoding_Unknown);
    CDB_VarChar(const char* s,
                EEncoding enc = eEncoding_Unknown);
    CDB_VarChar(const char* s,
                size_t l,
                EEncoding enc = eEncoding_Unknown);
    virtual ~CDB_VarChar(void);

    // assignment operators
    CDB_VarChar& operator= (const string& s)  { return SetValue(s); }
    CDB_VarChar& operator= (const char*   s)  { return SetValue(s); }

    // set-value methods
    CDB_VarChar& SetValue(const string& s,
                          EEncoding enc = eEncoding_Unknown);
    CDB_VarChar& SetValue(const char* s,
                          EEncoding enc = eEncoding_Unknown);
    CDB_VarChar& SetValue(const char* s, size_t l,
                          EEncoding enc = eEncoding_Unknown);

    //
    const char* Value() const  { return m_Null ? 0 : m_Val;  }
    size_t      Size()  const  { return m_Size; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(CDB_Object& v);

protected:
    size_t m_Size;
    char   m_Val[256];
};

class NCBI_DBAPIDRIVER_EXPORT CDB_Char : public CDB_String
{
public:
    enum { kMaxCharSize = 255 };

    CDB_Char(size_t s = 1);
    CDB_Char(size_t s,
             const string& v,
             EEncoding enc = eEncoding_Unknown);
    // This ctoe copies a string.
    CDB_Char(size_t len,
             const char* str,
             EEncoding enc = eEncoding_Unknown);
    CDB_Char(const CDB_Char& v);

    CDB_Char& operator= (const CDB_Char& v);
    CDB_Char& operator= (const string& v);
    // This operator copies a string.
    CDB_Char& operator= (const char* v);

    // This method copies a string.
    void SetValue(const char* str,
                  size_t len,
                  EEncoding enc = eEncoding_Unknown);

    //
    const char* Value() const  { return m_Null ? 0 : m_Val; }
    size_t      Size()  const  { return m_Size; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

    virtual void AssignValue(CDB_Object& v);
    virtual ~CDB_Char();

protected:
    size_t      m_Size;
    char*       m_Val;
};

#define K8_1 8191

class NCBI_DBAPIDRIVER_EXPORT CDB_LongChar : public CDB_String
{
public:

    CDB_LongChar(size_t s = K8_1);
    CDB_LongChar(size_t s,
                 const string& v,
                 EEncoding enc = eEncoding_Unknown);
    // This ctor copies a string.
    CDB_LongChar(size_t len,
                 const char* str,
                 EEncoding enc = eEncoding_Unknown);
    CDB_LongChar(const CDB_LongChar& v);

    CDB_LongChar& operator= (const CDB_LongChar& v);
    CDB_LongChar& operator= (const string& v);
    // This operator copies a string.
    CDB_LongChar& operator= (const char* v);

    // This method copies a string.
    void SetValue(const char* str,
                  size_t len,
                  EEncoding enc = eEncoding_Unknown);

    //
    const char* Value() const  { return m_Null ? 0 : m_Val; }
    size_t      Size()  const  { return m_Size; }
    size_t  DataSize()  const  { return m_Null? 0 : strlen(m_Val); }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

    virtual void AssignValue(CDB_Object& v);
    virtual ~CDB_LongChar();

protected:
    size_t      m_Size;
    char*       m_Val;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_VarBinary : public CDB_Object
{
public:
    CDB_VarBinary()                         : CDB_Object(true)  { return; }
    CDB_VarBinary(const void* v, size_t l)  { SetValue(v, l); }

    void SetValue(const void* v, size_t l);

    //
    const void* Value() const  { return m_Null ? 0 : (void*) m_Val; }
    size_t      Size()  const  { return m_Null ? 0 : m_Size; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(CDB_Object& v);

protected:
    size_t        m_Size;
    unsigned char m_Val[255];
};



class NCBI_DBAPIDRIVER_EXPORT CDB_Binary : public CDB_Object
{
public:
    enum { kMaxBinSize = 255 };

    CDB_Binary(size_t s = 1);
    CDB_Binary(size_t s, const void* v, size_t v_size);
    CDB_Binary(const CDB_Binary& v);

    void SetValue(const void* v, size_t v_size);

    CDB_Binary& operator= (const CDB_Binary& v);

    //
    const void* Value() const  { return m_Null ? 0 : (void*) m_Val; }
    size_t      Size()  const  { return m_Size; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

    virtual void AssignValue(CDB_Object& v);
    virtual ~CDB_Binary();

protected:
    size_t         m_Size;
    unsigned char* m_Val;
};

class NCBI_DBAPIDRIVER_EXPORT CDB_LongBinary : public CDB_Object
{
public:

    CDB_LongBinary(size_t s = K8_1);
    CDB_LongBinary(size_t s, const void* v, size_t v_size);
    CDB_LongBinary(const CDB_LongBinary& v);

    void SetValue(const void* v, size_t v_size);

    CDB_LongBinary& operator= (const CDB_LongBinary& v);

    //
    const void* Value() const  { return m_Null ? 0 : (void*) m_Val; }
    size_t      Size()  const  { return m_Size; }
    size_t  DataSize()  const  { return m_DataSize; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;

    virtual void AssignValue(CDB_Object& v);
    virtual ~CDB_LongBinary();

protected:
    size_t         m_Size;
    size_t         m_DataSize;
    unsigned char* m_Val;
};


class NCBI_DBAPIDRIVER_EXPORT CDB_Float : public CDB_Object
{
public:
    CDB_Float()        : CDB_Object(true)             { return; }
    CDB_Float(float i) : CDB_Object(false), m_Val(i)  { return; }

    CDB_Float& operator= (const float& i);

    float Value()   const { return m_Null ? 0 : m_Val; }
    void* BindVal() const { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(CDB_Object& v);

protected:
    float m_Val;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_Double : public CDB_Object
{
public:
    CDB_Double()         : CDB_Object(true)             { return; }
    CDB_Double(double i) : CDB_Object(false), m_Val(i)  { return; }

    CDB_Double& operator= (const double& i);

    //
    double Value()   const  { return m_Null ? 0 : m_Val; }
    void*  BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(CDB_Object& v);

protected:
    double m_Val;
};


class CMemStore;


class NCBI_DBAPIDRIVER_EXPORT CDB_Stream : public CDB_Object
{
public:
    // assignment
    virtual void AssignNULL();
    CDB_Stream&  Assign(const CDB_Stream& v);

    // data manipulations
    virtual size_t Read     (void* buff, size_t nof_bytes);
    virtual size_t Append   (const void* buff, size_t nof_bytes);
    virtual void   Truncate (size_t nof_bytes = kMax_Int);
    virtual bool   MoveTo   (size_t byte_number);

    // current size of data
    virtual size_t Size() const;
    virtual void AssignValue(CDB_Object& v);

protected:
    // 'ctors
    CDB_Stream();
    virtual ~CDB_Stream();

private:
    // data storage
    CMemStore* m_Store;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_Image : public CDB_Stream
{
public:
    CDB_Image& operator= (const CDB_Image& image);

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_Text : public CDB_Stream
{
public:
    virtual size_t Append(const void* buff, size_t nof_bytes = 0/*strlen*/);
    virtual size_t Append(const string& s);

    CDB_Text& operator= (const CDB_Text& text);

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_SmallDateTime : public CDB_Object
{
public:
    CDB_SmallDateTime(CTime::EInitMode mode= CTime::eEmpty);
    CDB_SmallDateTime(const CTime& t);
    CDB_SmallDateTime(Uint2 days, Uint2 minutes);

    CDB_SmallDateTime& operator= (const CTime& t);

    CDB_SmallDateTime& Assign(Uint2 days, Uint2 minutes);
    const CTime& Value(void) const;
    Uint2 GetDays(void) const;
    Uint2 GetMinutes(void) const;

    virtual EDB_Type    GetType(void) const;
    virtual CDB_Object* Clone(void)   const;
    virtual void AssignValue(CDB_Object& v);

protected:
    mutable CTime        m_NCBITime;
    mutable TDBTimeU     m_DBTime;
    // which of m_NCBITime(0x1), m_DBTime(0x2) is valid;  they both can be valid
    mutable unsigned int m_Status;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_DateTime : public CDB_Object
{
public:
    CDB_DateTime(CTime::EInitMode mode= CTime::eEmpty);
    CDB_DateTime(const CTime& t);
    CDB_DateTime(Int4 d, Int4 s300);

    CDB_DateTime& operator= (const CTime& t);

    CDB_DateTime& Assign(Int4 d, Int4 s300);
    const CTime& Value(void) const;

    Int4 GetDays(void) const;
    Int4 Get300Secs(void) const;

    virtual EDB_Type    GetType(void) const;
    virtual CDB_Object* Clone(void)   const;
    virtual void AssignValue(CDB_Object& v);

protected:
    mutable CTime        m_NCBITime;
    mutable TDBTimeI     m_DBTime;
    // which of m_NCBITime(0x1), m_DBTime(0x2) is valid;  they both can be valid
    mutable unsigned int m_Status;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_Bit : public CDB_Object
{
public:
    CDB_Bit()       : CDB_Object(true)   { return; }
    CDB_Bit(int  i) : CDB_Object(false)  { m_Val = i ? 1 : 0; }
    CDB_Bit(bool i) : CDB_Object(false)  { m_Val = i ? 1 : 0; }

    CDB_Bit& operator= (const int& i);
    CDB_Bit& operator= (const bool& i);

    int   Value()   const  { return m_Null ? 0 : (int) m_Val; }
    void* BindVal() const  { return (void*) &m_Val; }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(CDB_Object& v);

protected:
    Uint1 m_Val;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_Numeric : public CDB_Object
{
public:
    CDB_Numeric();
    CDB_Numeric(unsigned int precision, unsigned int scale);
    CDB_Numeric(unsigned int precision, unsigned int scale,
                const unsigned char* arr);
    CDB_Numeric(unsigned int precision, unsigned int scale, bool is_negative,
                const unsigned char* arr);
    CDB_Numeric(unsigned int precision, unsigned int scale, const char* val);
    CDB_Numeric(unsigned int precision, unsigned int scale, const string& val);

    CDB_Numeric& Assign(unsigned int precision, unsigned int scale,
                        const unsigned char* arr);
    CDB_Numeric& Assign(unsigned int precision, unsigned int scale,
                        bool is_negative, const unsigned char* arr);

    CDB_Numeric& operator= (const char* val);
    CDB_Numeric& operator= (const string& val);

    string Value() const;

    Uint1 Precision() const {
        return m_Precision;
    }

    Uint1 Scale() const {
        return m_Scale;
    }

    virtual EDB_Type    GetType() const;
    virtual CDB_Object* Clone()   const;
    virtual void AssignValue(CDB_Object& v);

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


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.23  2006/09/13 19:28:56  ssikorsk
 * Added new classes CWString and CDB_String;
 * Revamp CDB_VarChar, CDB_Char, and CDB_LongChar to inherit from CDB_String;
 *
 * Revision 1.22  2006/08/22 20:12:44  ssikorsk
 * Reordered EDB_Type.
 *
 * Revision 1.21  2006/08/07 15:54:43  ssikorsk
 * Moved definitions of big inline functions into cpp.
 *
 * Revision 1.20  2005/09/21 13:54:19  ssikorsk
 * Set CDB_DateTime and CDB_SmallDateTime to null in case of initialization with empty CTime
 *
 * Revision 1.19  2005/07/12 19:19:13  soussov
 * fixes bug in CDB_[Small]DateTime constructor
 *
 * Revision 1.18  2005/05/31 21:01:52  ssikorsk
 * Added GetTypeName method to the CDB_Object class
 *
 * Revision 1.17  2003/05/13 16:54:40  sapojnik
 * CDB_Object::Create() - support for LongChar, LongBinary
 *
 * Revision 1.16  2003/05/05 15:56:15  soussov
 * new default size 8K-1 for CDB_LongChar, CDB_LongBinary
 *
 * Revision 1.15  2003/04/29 21:12:29  soussov
 * new datatypes CDB_LongChar and CDB_LongBinary added
 *
 * Revision 1.14  2003/04/11 17:46:11  siyan
 * Added doxygen support
 *
 * Revision 1.13  2003/01/30 16:06:04  soussov
 * Changes the default from eCurrent to eEmpty for DateTime types
 *
 * Revision 1.12  2002/12/26 19:29:12  dicuccio
 * Added Win32 export specifier for base DBAPI library
 *
 * Revision 1.11  2002/10/07 13:08:32  kans
 * repaired inconsistent newlines caught by Mac compiler
 *
 * Revision 1.10  2002/09/13 18:28:05  soussov
 * fixed bug with long overflow
 *
 * Revision 1.9  2002/05/16 21:27:01  soussov
 * AssignValue methods added
 *
 * Revision 1.8  2002/02/14 00:59:38  vakatov
 * Clean-up: warning elimination, fool-proofing, fine-tuning, identation, etc.
 *
 * Revision 1.7  2002/02/13 22:37:27  sapojnik
 * new_CDB_Object() renamed to CDB_Object::create()
 *
 * Revision 1.6  2002/02/13 22:14:50  sapojnik
 * new_CDB_Object() (needed for rdblib)
 *
 * Revision 1.5  2002/02/06 22:21:58  soussov
 * fixes the numeric default constructor
 *
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

